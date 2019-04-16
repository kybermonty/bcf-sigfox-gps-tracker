const express = require('express');
const bodyParser = require('body-parser');
const mqtt = require('mqtt');

const app = express();
app.use(bodyParser.urlencoded({ extended: false }));

const mqttClient = mqtt.connect();

const bytesToInt = (bytes) => {
	let i = 0;
	for (let x = 0; x < bytes.length; x++) {
		i |= +(bytes[x] << (x * 8));
	}
	return i;
};
const latLng = (bytes) => {
	if (bytes.length !== latLng.BYTES) {
		throw new Error('Lat/Long must have exactly 8 bytes');
	}

	const lat = bytesToInt(bytes.slice(0, latLng.BYTES / 2));
	const lng = bytesToInt(bytes.slice(latLng.BYTES / 2, latLng.BYTES));

	return [lat / 1e6, lng / 1e6];
};
latLng.BYTES = 8;

app.post('/sigfox-gps-tracker', (req, res) => {
	const dt = new Date(parseInt(req.body.time, 10) * 1000);
	console.log('time', dt.toISOString());

	const gps = latLng(Buffer.from(req.body.data, 'hex'));
	console.log('gps', gps);

	if (gps && gps.length === 2) {
		mqttClient.publish('node/car/location', JSON.stringify({ latitude: gps[0], longitude: gps[1] }));
	}

	res.sendStatus(204);
});

app.listen(8020);
