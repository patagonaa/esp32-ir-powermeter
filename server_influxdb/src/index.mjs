import * as mqtt from "mqtt";
import fetch from 'node-fetch';

const MQTT_SERVER = process.env.MQTT_SERVER || "mqtt://localhost";
const MQTT_USER = process.env.MQTT_USER;
const MQTT_PASSWORD = process.env.MQTT_PASSWORD;

const INFLUX_SERVER = process.env.INFLUX_SERVER || "http://localhost:8086";
const INFLUX_USER = process.env.INFLUX_USER || "";
const INFLUX_PASSWORD = process.env.INFLUX_PASSWORD || "";
const INFLUX_DATABASE = process.env.INFLUX_DATABASE || 'ir-powermeter';

function getLine(measurement, tags, values, timestamp_ns) {
	let body = measurement;
	if (tags) {
		for (let tag in tags) {
			body += `,${tag}=${tags[tag]}`;
		}
	}
	body += ' ';
	let first = true;
	for (let field in values) {
		if (!first) {
			body += ',';
		} else {
			first = false;
		}
		body += `${field}=${values[field]}`;
	}

	if (timestamp_ns != null) {
		body += ` ${timestamp_ns}`;
	}
	return body;
}

async function sendValues(lines) {
	let requestOptions = {
		method: 'POST',
		body: lines,
		headers: {
			'Authorization': 'Basic ' + Buffer.from(INFLUX_USER + ':' + INFLUX_PASSWORD).toString('base64')
		}
	};
	try {
		let response = await fetch(`${INFLUX_SERVER}/write?db=${INFLUX_DATABASE}`, requestOptions);
		if (!response.ok) {
			throw `status ${response.status}`;
		}
		console.info('successfully transmitted value: HTTP', response.status);
	} catch (error) {
		console.error("error while transmitting value: ", error);
	}
}

const mqttClient = mqtt.connect(MQTT_SERVER, { username: MQTT_USER, password: MQTT_PASSWORD });

mqttClient.on('connect', function () {
	mqttClient.subscribe('ir-powermeter/+/+');
});

async function handleData(clientId, data) {
	let timestamp_ns = Date.now() * 1000 * 1000;

	let registers = {};
	for (let line of data.toString().split(/\r?\n/)) {
		var matches = /(.*)\((.*?)(?:\*(\w+))?\)/.exec(line);
		if (!matches)
			continue;
		var obisRegister = matches[1];
		registers[obisRegister] = matches[2];
	}

	let influxLines = [];

	if (registers["1.8.0"] != null) {
		influxLines.push(getLine('total', { clientid: clientId, direction: 'pos' }, { kwh: parseFloat(registers["1.8.0"]) }, timestamp_ns));
	}
	if (registers["2.8.0"] != null) {
		influxLines.push(getLine('total', { clientid: clientId, direction: 'neg' }, { kwh: parseFloat(registers["2.8.0"]) }, timestamp_ns));
	}
	if (registers["1.8.0"] != null && registers["2.8.0"] != null) {
		influxLines.push(getLine('total_normalized', { clientid: clientId }, { kwh: (parseFloat(registers["1.8.0"]) - parseFloat(registers["2.8.0"])) }, timestamp_ns));
	}

	let phases = [
		{ name: "L1", volts: "32.7", amps: "31.7" },
		{ name: "L2", volts: "52.7", amps: "51.7" },
		{ name: "L3", volts: "72.7", amps: "71.7" }
	];

	for (let phaseInfo of phases) {
		if (registers[phaseInfo.volts] != null && registers[phaseInfo.amps] != null) {
			let values = {
				volts: parseFloat(registers[phaseInfo.volts]),
				amps: parseFloat(registers[phaseInfo.amps]),
				voltamperes: parseFloat(registers[phaseInfo.volts]) * parseFloat(registers[phaseInfo.amps])
			};
			influxLines.push(getLine('current', { clientid: clientId, phase: phaseInfo.name }, values, timestamp_ns));
		}
	}

	await sendValues(influxLines.join('\n'))
}

mqttClient.on('message', async function (topic, message) {
	let topicSplit = topic.split('/');
	let clientId = topicSplit[1];
	let messageType = topicSplit[2];
	switch (messageType) {
		case 'data':
			handleData(clientId, message);
			console.info(message.toString());
			break;
		case 'temperature_c':
			let temperature = parseFloat(message.toString());
			console.info('Got', temperature, '°C from', clientId);
			await sendValues(getLine('temperature', { clientid: clientId }, { value: temperature }));
			break;
		case 'uptime_ms':
			let uptime = parseFloat(message.toString());
			console.info('Got', uptime, 'ms uptime from', clientId);
			await sendValues(getLine('uptime', { clientid: clientId }, { value: uptime }));
			break;
		case 'measurement_time_ms':
			let measurementTime = parseFloat(message.toString());
			console.info('Got', measurementTime, 'ms measurement time from', clientId);
			await sendValues(getLine('measurement_time', { clientid: clientId }, { value: measurementTime }));
			break;
		case 'dead':
			console.info('client', clientId, 'died with message', message.toString());
			break;
		case 'alive':
			console.info('client', clientId, 'went alive with message', message.toString());
			break;
		default:
			console.warn('unknown messageType', messageType);
			break;
	}
});