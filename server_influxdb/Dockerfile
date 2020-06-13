FROM node:14
ENV NODE_ENV="production"
WORKDIR /usr/src/app
COPY package.json package-lock.json ./
RUN npm install
COPY src ./src

CMD ["node", "./src/index.js"]
