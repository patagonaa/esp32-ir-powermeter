const mqtt = require('mqtt');
const request = require('request');

const MQTT_SERVER = process.env.MQTT_SERVER || "mqtt://localhost";
const MQTT_USER = process.env.MQTT_USER;
const MQTT_PASSWORD = process.env.MQTT_PASSWORD;

const INFLUX_SERVER = process.env.INFLUX_SERVER || "http://localhost:8086";
const INFLUX_DATABASE = process.env.INFLUX_DATABASE || 'ir-powermeter';

function getLine(measurement, tags, values, timestamp_ns){
	let body = measurement;
	if(tags){
		for(let tag in tags){
			body += `,${tag}=${tags[tag]}`;
		}
	}
	body += ' ';
	let first = true;
	for(let field in values){
		if(!first){
			body += ',';
		}else{
			first = false;
		}
		body += `${field}=${values[field]}`;
	}
	
	body += ` ${timestamp_ns}`;
	return body;
}

function sendValues(lines) {	
    let requestOptions = {
        body: lines
    };
    request.post(`${INFLUX_SERVER}/write?db=${INFLUX_DATABASE}`, requestOptions, (error, response, body) => {
        if (error) {
            console.error("error while transmitting value: ", error);
        } else if (response.statusCode >= 200 && response.statusCode < 300) {
            console.info('successfully transmitted value: HTTP', response.statusCode);
        } else {
            console.error('error while transmitting value: HTTP', response.statusCode);
        }
    });
}

const mqttClient = mqtt.connect(MQTT_SERVER, { username: MQTT_USER, password: MQTT_PASSWORD });

mqttClient.on('connect', function () {
    mqttClient.subscribe('ir-powermeter/+/+');
});

function handleData(clientId, data){
	let timestamp_ns = Date.now() * 1000 * 1000;
	var influxLines = [];
	for(let line of data.toString().split(/\r?\n/)){
		var matches = /(.*)\((.*?)(?:\*(\w+))?\)/.exec(line);
		if(!matches)
			continue;
		var obisRegister = matches[1];
		switch(obisRegister){
			case "1.8.0":
				influxLines.push(getLine('total', {clientid: clientId, direction: 'pos'}, {kwh: parseFloat(matches[2])}, timestamp_ns));
				break;
			case "2.8.0":
				influxLines.push(getLine('total', {clientid: clientId, direction: 'neg'}, {kwh: parseFloat(matches[2])}, timestamp_ns));
				break;
			case "32.7":
				influxLines.push(getLine('current', {clientid: clientId, phase: 'L1'}, {volts: parseFloat(matches[2])}, timestamp_ns));
				break;
			case "52.7":
				influxLines.push(getLine('current', {clientid: clientId, phase: 'L2'}, {volts: parseFloat(matches[2])}, timestamp_ns));
				break;
			case "72.7":
				influxLines.push(getLine('current', {clientid: clientId, phase: 'L3'}, {volts: parseFloat(matches[2])}, timestamp_ns));
				break;
			case "31.7":
				influxLines.push(getLine('current', {clientid: clientId, phase: 'L1'}, {amps: parseFloat(matches[2])}, timestamp_ns));
				break;
			case "51.7":
				influxLines.push(getLine('current', {clientid: clientId, phase: 'L2'}, {amps: parseFloat(matches[2])}, timestamp_ns));
				break;
			case "71.7":
				influxLines.push(getLine('current', {clientid: clientId, phase: 'L3'}, {amps: parseFloat(matches[2])}, timestamp_ns));
				break;
		}
	}
	sendValues(influxLines.join('\n'))
}

mqttClient.on('message', function (topic, message) {
    let topicSplit = topic.split('/');
    let clientId = topicSplit[1];
    let messageType = topicSplit[2];
    switch (messageType) {
        case 'data':
			handleData(clientId, message);
			console.info(message.toString());
            break;
        case 'dead':
            console.info('client', clientId, 'died with message', message.toString());
            break;
        default:
            console.warn('unknown messageType', messageType);
            break;
    }
});