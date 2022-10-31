import React, { useRef } from 'react';
import { Editable, Flash, Popup, Post } from './Reuseables.js';
import logo from './logo.svg';
import './App.css';

let ip5 = "http://192.168.5.1/"; // ly 5up
let ip6 = "http://192.168.6.1/"; // ly 5up

function searchIPS(cb) {
	return new Promise((resolve, reject) => {
		let iptobitarr = (ipstr) => {
			let toreturn = ipstr.split(".");
			return toreturn.map((x) => {
				let ret = parseInt(x);
				let retarr = [];
				for (let i = 0; i < 8; i++) {
					retarr.push((ret & (1 << i)) >> i);
				}
				return retarr;
			});
		}

		let process = (networkresolution) => {
			// split up stuff
			let ipsplit = iptobitarr(networkresolution.ip);
			let subnetsplit = iptobitarr(networkresolution.subnet);

			let switchers = [];
			for (let i = 0; i < 4; i++) {
				for (let j = 0; j < 8; j++) {
					if (subnetsplit[i][j] == 0) {
						switchers.push([i, j]);
					}
				}
			}

			for (let i = 0; i < (1 << switchers.length); i++) {
				for (let j = 0; j < switchers.length; j++) {
					ipsplit[switchers[j][0]][switchers[j][1]] = (((1 << j) & i) != 0) ? 1 : 0;
				}
				// construct it back
				let numarr = [];
				for (let k = 0; k < 4; k++) {
					let subt = 0;
					for (let l = 0; l < 8; l++) {
						subt = subt | (ipsplit[k][l] << l);
					}
					numarr.push(subt);
				}
				cb("http://" + numarr.join(".") + "/");
			}
		}

		networkinterface.getWiFiIPAddress((ipinformation) => resolve(process(ipinformation)), (err) => {
			console.log(err);
			resolve(process({
				ip: "0.0.0.0",
				subnet: "255.255.255.255"
			}));
		})
	});
}

function ipSpace() {
	return new Promise((resolve, reject) => {
		let iptobitarr = (ipstr) => {
			let toreturn = ipstr.split(".");
			return toreturn.map((x) => {
				let ret = parseInt(x);
				let retarr = [];
				for (let i = 0; i < 8; i++) {
					retarr.push((ret & (1 << i)) >> i);
				}
				return retarr;
			});
		}

		let process = (networkresolution) => {
			// split up stuff
			let subnetsplit = iptobitarr(networkresolution.subnet);

			let switchers = [];
			for (let i = 0; i < 4; i++) {
				for (let j = 0; j < 8; j++) {
					if (subnetsplit[i][j] == 0) {
						switchers.push([i, j]);
					}
				}
			}

			return (1 << switchers.length);
		}
		networkinterface.getWiFiIPAddress((ipinformation) => resolve(process(ipinformation)), (err) => {
			console.log(err);
			resolve(process({
				ip: "0.0.0.0",
				subnet: "255.255.255.255"
			}));
		})
	});
}

function rgbtohsv(r) {
	let g, b;
	if (arguments.length === 1) {
		g = r.g, b = r.b, r = r.r;
	}
	var max = Math.max(r, g, b), min = Math.min(r, g, b),
		d = max - min,
		h,
		s = (max === 0 ? 0 : d / max),
		v = max / 255;

	switch (max) {
		case min: h = 0; break;
		case r: h = (g - b) + d * (g < b ? 6 : 0); h /= 6 * d; break;
		case g: h = (b - r) + d * 2; h /= 6 * d; break;
		case b: h = (r - g) + d * 4; h /= 6 * d; break;
	}

	return {
		r: Math.round(h * 255),
		g: Math.round(s * 255),
		b: Math.round(v * 255)
	};
}

function hsvtorgb(h) {
	var r, g, b, i, f, p, q, t, s, v;
	if (arguments.length === 1) {
		s = h.g, v = h.b, h = h.r;
	}
	h /= 255;
	s /= 255;
	v /= 255;
	i = Math.floor(h * 6);
	f = h * 6 - i;
	p = v * (1 - s);
	q = v * (1 - f * s);
	t = v * (1 - (1 - f) * s);
	switch (i % 6) {
		case 0: r = v, g = t, b = p; break;
		case 1: r = q, g = v, b = p; break;
		case 2: r = p, g = v, b = t; break;
		case 3: r = p, g = q, b = v; break;
		case 4: r = t, g = p, b = v; break;
		case 5: r = v, g = p, b = q; break;
	}

	return {
		r: Math.round(r * 255),
		g: Math.round(g * 255),
		b: Math.round(b * 255)
	};
}

function millisecondsToStr(milliseconds) {
	// TIP: to find current time in milliseconds, use:
	// var  current_time_milliseconds = new Date().getTime();

	function numberEnding(number) {
		return (number > 1) ? 's' : '';
	}

	var temp = Math.floor(milliseconds / 1000);
	var years = Math.floor(temp / 31536000);
	if (years) {
		return years + ' year' + numberEnding(years);
	}
	//TODO: Months! Maybe weeks? 
	var days = Math.floor((temp %= 31536000) / 86400);
	if (days) {
		return days + ' day' + numberEnding(days);
	}
	var hours = Math.floor((temp %= 86400) / 3600);
	if (hours) {
		return hours + ' hour' + numberEnding(hours);
	}
	var minutes = Math.floor((temp %= 3600) / 60);
	if (minutes) {
		return minutes + ' minute' + numberEnding(minutes);
	}
	var seconds = temp % 60;
	if (seconds) {
		return seconds + ' second' + numberEnding(seconds);
	}
	return 'less than a second'; //'just now' //or other string you like;
}

function devicebyindex(garden, index) {
	if (index == 0) {
		return garden.master;
	}
	else {
		return garden.devices[index - 1]
	}
}

function timeout(ms, promise) {
	return new Promise((resolve, reject) => {
		const timer = setTimeout(() => {
			reject(new Error('TIMEOUT'))
		}, ms)

		promise
			.then(value => {
				clearTimeout(timer)
				resolve(value)
			})
			.catch(reason => {
				clearTimeout(timer)
				reject(reason)
			})
	})
}

function makePostOptions(json) {
	return {
		method: 'post',
		body: new URLSearchParams(json).toString(),
		headers: {
			'Content-Type': 'application/x-www-form-urlencoded',
			'Origin': '',
			'Referer': ''
		}
	};
}

let getOptions = {
	method: 'get',
	headers: {
		'Origin': '',
		'Referer': ''
	}
}

function isBoardname(response) {
	return (response.length == "AutoGarden-XXXXXXXX".length && response.substr(0, "AutoGarden-".length) == "AutoGarden-");
}

function isSettings(response) {
	try {
		return isBoardname(JSON.parse(response).boardname);
	}
	catch (e) {
		return false;
	}
}

let spread = 100;
function getMoistureSetting(settings) {
	if (settings.moistureEnd < 4095 - spread) {
		return settings.moistureEnd - spread;
	}
	else {
		return settings.moistureBegin + spread;
	}
}

function lightsOn(settings) {
	// console.log(settings);
	try {
		if (settings.lowlighting.r == 0 && settings.lowlighting.g == 0 && settings.lowlighting.b == 0 &&
			settings.highlighting.r == 0 && settings.highlighting.g == 0 && settings.highlighting.b == 0) {
			return false
		}
	}
	catch (e) { }
	return true;
}

function wateringOn(settings) {
	try {
		return (settings.moistureBegin < 4096)
	}
	catch (e) { }
	return true;
}

function getForwardData(response) {
	let respdata = response.split("\n");
	return {
		code: parseInt(respdata.shift()),
		response: respdata.join("\n")
	}
}

let contextval = {
	"gardens": [
		{
			"name": "New Garden",
			"master": {
				"master": true,
				"humanname": "AutoGarden-HEV1OE4M",
				"boardname": "AutoGarden-HEV1OE4M",
				"lastip": "http://192.168.1.181/",
				"mac": "C4:4F:33:75:A7:31",
				"connInfo": {
					"connected": true
				},
				"settings": {
					"boardname": "AutoGarden-HEV1OE4M",
					"version": "0.1",
					"isSlave": false,
					"hasWiFi": true,
					"moistureBegin": 500,
					"moistureEnd": 2000,
					"checkingTime": 1,
					"cooldown": 2,
					"lightingtype": 98,
					"lowlighting": {
						"r": 255,
						"g": 0,
						"b": 0
					},
					"highlighting": {
						"r": 0,
						"g": 0,
						"b": 255
					},
					"lowvalue": 100,
					"highvalue": 4096
				},
				"readings": {
					"moisture": 0,
					"temperature": 0,
					"sunlight": 0
				},
				"watering": {
					"watering": false,
					"reason": '_',
					"cooldownleft": -1
				}
			},
			"devices": [
				{
					"master": false,
					"humanname": "AutoGarden-ST31LMY3",
					"boardname": "AutoGarden-ST31LMY3",
					"lastip": null,
					"mac": "C4:4F:33:75:7A:B1",
					"connInfo": {
						"connected": true
					},
					"settings": {
						"boardname": "AutoGarden-ST31LMY3",
						"version": "0.1",
						"isSlave": true,
						"hasWiFi": true,
						"moistureBegin": 5000,
						"moistureEnd": 5000,
						"checkingTime": 5,
						"cooldown": 60,
						"lightingtype": 115,
						"lowlighting": {
							"r": 0,
							"g": 0,
							"b": 0
						},
						"highlighting": {
							"r": 0,
							"g": 0,
							"b": 0
						},
						"lowvalue": 0,
						"highvalue": 4096
					},
					"readings": {
						"moisture": 0,
						"temperature": 0,
						"sunlight": 0
					},
					"watering": {
						"watering": false,
						"reason": '_',
						"cooldownleft": -1
					}
				}
			]
		}
	],
	"networkSSID": "VFConnect",
	"slide": "Home"
};
const globalStateContext = React.createContext(contextval);

function masterConnected(garden) {
	return garden.master.connInfo.connected;
}

class HeaderRow extends React.Component {
	static contextType = globalStateContext;
	constructor(props) {
		super(props);
	}

	render() {
		return (
			<div className="header-box p-3">
				<div className="row m-0 align-items-center h-100">
					<div className="col-auto">
						<h4 className="m-0">AutoGarden v0.1</h4>
					</div>
					<div className="col-auto ml-auto text-muted">
						<i onClick={this.props.showSettings} className="material-icons">settings</i>
					</div>
				</div>
			</div>
		)
	}
}

class Device extends React.Component {
	static contextType = globalStateContext;
	constructor(props) {
		super(props);
		this.updateConnInfo = this.updateConnInfo.bind(this);
		this.updateSensorData = this.updateSensorData.bind(this);
		this.changelightingmode = this.changelightingmode.bind(this);
		this.startWatering = this.startWatering.bind(this);
		this.stopWatering = this.stopWatering.bind(this);
		this.saveMoistureSettings = this.saveMoistureSettings.bind(this);
		this.saveLightingSettings = this.saveLightingSettings.bind(this);
		this.saveAdvancedSettings = this.saveAdvancedSettings.bind(this);
		this.delete = this.delete.bind(this);
		this.datasync = React.createRef();
		this.settingsCPS
		this.popup = React.createRef();
		this._flashref = React.createRef();
		this.lightingposts = React.createRef();
		this.moistureposts = React.createRef();
		this.wateringposts = React.createRef();
		this.advancedposts = React.createRef();
		this.deletionpost = React.createRef();
		this.state = {
			settingsCPS: JSON.parse(JSON.stringify(this.props.device.settings))
		}
		this.datainterval = setInterval(() => {
			// try to access the master board
			// in other words, use our update gardens to update connection info
			// TODO: if lastip doesnt match, silently run a scan to update the ip
			this.updateConnInfo();
		}, (this.props.index == 0 ? 10000 : 15000));
		this.sensorinterval = 0;
	}

	componentWillUnmount() {
		console.log("UNMOUNTING");
		clearInterval(this.datainterval);
		clearInterval(this.sensorinterval);
	}


	updateConnInfo() {
		// TODO: support for local connections
		// TODO: dry
		console.log("conninfoupdated")
		if (this.props.index == 0) {
			timeout(5000, fetch(this.props.master.lastip + "settings", getOptions).then(response => response.text()).then((response) => {
				console.log("fetchsuccess");
				console.log(response);
				console.log(this.props.master);
				this.props.device.connInfo.connected = isSettings(response) && JSON.parse(response).boardname == this.props.master.boardname;
				this.props.master.settings = JSON.parse(response);
				this.props.updateDevice(this.props.device);	// TODO: add a message to conninfo for some human readable debug
			})).catch((err) => {
				console.log("errhere");
				console.log(err);
				this.props.device.connInfo.connected = false;
				this.props.updateDevice(this.props.device);	// TODO: add a message to conninfo for some human readable debug
			}).finally(() => {
				console.log("flaadshededefe");
				this._flashref.current.flash();
				this.props.flashmaster();
			});
		}
		else {
			let promisemaker = (url) => {
				return timeout(5000, fetch(url + "forward", makePostOptions({
					boardname: this.props.device.boardname,
					postjson: "null",
					post: "0",
					url: "/settings"
				}))).then(response => response.text()).then((response) => {
					console.log(JSON.parse(getForwardData(response).response));
					return getForwardData(response).response;
				}).catch((err) => {
					return false
				});
			}
			Promise.all([promisemaker(this.props.master.lastip), promisemaker(ip5), promisemaker(ip6)]).then((values) => {
				let settingsmatches = (response) => {
					return isSettings(response) && JSON.parse(response).boardname == this.props.device.boardname;
				}
				console.log("inhereeeeeeee");
				console.log(values);
				this.props.device.connInfo.connected = settingsmatches(values[0]) || settingsmatches(values[1]) || settingsmatches(values[2]);
				if (this.props.device.connInfo.connected) {
					let mtched = -1;
					for (let i = 0; i < 3; i++) {
						if (settingsmatches(values[i])) {
							mtched = i;
							this.props.device.settings = JSON.parse(values[i]);
						};
					}
				}
				this.props.updateDevice(this.props.device);
				this._flashref.current.flash();
				this.props.flashmaster();
			})
		}
	}

	updateSensorData() { // TODO: dry
		let index = this.props.index;
		let processfunc = (response) => {
			console.log(response);
			let sensorarr = response.split("\n");
			this.props.device.readings.temperature = Math.round((parseFloat(sensorarr[0]) - 2500) / 10) / 10;
			this.props.device.readings.moisture = Math.min(100, Math.max(0, Math.round(100 * (parseFloat(sensorarr[1]) / 4095))));
			this.props.device.readings.sunlight = Math.min(100, Math.max(0, Math.round(100 * (parseFloat(sensorarr[2]) / 4095))));
			console.log(this.props.device.readings);

			this.props.device.watering.watering = (sensorarr[4] == "watering");
			this.props.device.watering.reason = sensorarr[5];
			this.props.device.watering.cooldownleft = parseFloat(sensorarr[6]);
			console.log(this.props.device.readings);
			this.datasync.current.flash();
			this.props.updateDevice(this.props.device);
		}

		if (index == 0) {
			timeout(3000, fetch(this.props.master.lastip + "sensordata", getOptions)).then(response => response.text()).then((response) => {
				processfunc(response);
			});
		}
		else {
			timeout(3000, fetch(this.props.master.lastip + "forward", makePostOptions({
				boardname: this.props.device.boardname,
				postjson: "null",
				post: "0",
				url: "/sensordata"
			}))).then(response => response.text()).then(response => { console.log(response); return getForwardData(response).response; }).then((response) => {
				processfunc(response)
			});
		}
	}

	changelightingmode(e) {
		this.state.settingsCPS.lightingtype = parseInt(e.target.value);
		this.setState({ settingsCPS: this.state.settingsCPS });
	}

	startWatering() {
		this.wateringposts.current.begin();
		let moisturepostjson = {
			force: "1",
			for: "6"
		}
		let fetchmasterconst = () => {
			return fetch(this.props.master.lastip + "userwater", makePostOptions(moisturepostjson)).then(response => response.text())
		}
		let fetchslaveconst = () => {
			return fetch(this.props.master.lastip + "forward", makePostOptions({
				boardname: this.props.device.boardname,
				postjson: new URLSearchParams(moisturepostjson).toString(),
				post: "1",
				url: "/userwater"
			})).then(response => response.text()).then(response => {
				return getForwardData(response).response;
			})
		}
		let fetchfunc;
		if (this.props.index == 0) fetchfunc = fetchmasterconst();
		else fetchfunc = fetchslaveconst();
		timeout(5000, fetchfunc).then((rsp) => {
			console.log(rsp);
			if (rsp == "OK") {
				this.wateringposts.current.succeed();
			}
			else {
				this.wateringposts.current.fail();
			}
		})
	}

	stopWatering() {
		this.wateringposts.current.begin();
		let moisturepostjson = {}
		let fetchmasterconst = () => {
			return fetch(this.props.master.lastip + "userstopwater", makePostOptions(moisturepostjson)).then(response => response.text())
		}
		let fetchslaveconst = () => {
			return fetch(this.props.master.lastip + "forward", makePostOptions({
				boardname: this.props.device.boardname,
				postjson: new URLSearchParams(moisturepostjson).toString(),
				post: "1",
				url: "/userstopwater"
			})).then(response => response.text()).then(response => {
				return getForwardData(response).response;
			})
		}
		let fetchfunc;
		if (this.props.index == 0) fetchfunc = fetchmasterconst();
		else fetchfunc = fetchslaveconst();
		timeout(5000, fetchfunc).then((rsp) => {
			if (rsp == "OK") {
				this.wateringposts.current.succeed();
			}
			else {
				this.wateringposts.current.fail();
			}
		})
	}

	saveMoistureSettings() {
		this.moistureposts.current.begin();
		let moisturepostjson = {
			moistureBegin: this.state.settingsCPS.moistureBegin,
			moistureEnd: this.state.settingsCPS.moistureEnd
		}
		let fetchmasterconst = () => {
			return fetch(this.props.master.lastip + "moisturesettings", makePostOptions(moisturepostjson)).then(response => response.text())
		}
		let fetchslaveconst = () => {
			return fetch(this.props.master.lastip + "forward", makePostOptions({
				boardname: this.props.device.boardname,
				postjson: new URLSearchParams(moisturepostjson).toString(),
				post: "1",
				url: "/moisturesettings"
			})).then(response => response.text()).then(response => {
				return getForwardData(response).response;
			})
		}
		let fetchfunc;
		if (this.props.index == 0) fetchfunc = fetchmasterconst();
		else fetchfunc = fetchslaveconst();
		timeout(5000, fetchfunc).then((rsp) => {
			if (rsp == "OK") {
				this.moistureposts.current.succeed();
				// save it
				this.props.device.settings.moistureBegin = moisturepostjson.moistureBegin;
				this.props.device.settings.moistureEnd = moisturepostjson.moistureEnd;
				this.props.updateDevice(this.props.device);
			}
			else {
				this.moistureposts.current.fail();
			}
		})
	}

	saveAdvancedSettings() {
		this.advancedposts.current.begin();
		let moisturepostjson = {
			checkingTime: this.state.settingsCPS.checkingTime,
			cooldown: this.state.settingsCPS.cooldown
		}
		let fetchmasterconst = () => {
			return fetch(this.props.master.lastip + "advancedsettings", makePostOptions(moisturepostjson)).then(response => response.text())
		}
		let fetchslaveconst = () => {
			return fetch(this.props.master.lastip + "forward", makePostOptions({
				boardname: this.props.device.boardname,
				postjson: new URLSearchParams(moisturepostjson).toString(),
				post: "1",
				url: "/advancedsettings"
			})).then(response => response.text()).then(response => {
				return getForwardData(response).response;
			})
		}
		let fetchfunc;
		if (this.props.index == 0) fetchfunc = fetchmasterconst();
		else fetchfunc = fetchslaveconst();
		timeout(5000, fetchfunc).then((rsp) => {
			if (rsp == "OK") {
				this.advancedposts.current.succeed();
				// save it
				this.props.device.settings.checkingTime = moisturepostjson.checkingTime;
				this.props.device.settings.cooldown = moisturepostjson.cooldown;
				this.props.updateDevice(this.props.device);
			}
			else {
				this.advancedposts.current.fail();
			}
		})
	}

	saveLightingSettings() {
		this.lightingposts.current.begin();
		let loval = this.state.settingsCPS.moistureBegin;
		let hival = this.state.settingsCPS.moistureEnd;
		let ltype = "b";
		switch (this.state.settingsCPS.lightingtype) {
			case 97: // "dark"
				loval = 100;
				hival = 4000;
				ltype = "a";
				break;
			case 99: // "cold"
				loval = 100;
				hival = 4000;
				ltype = "c";
				break;
		}
		let lightingpostjson = {
			lightingtype: ltype,
			lowvalue: loval,
			highvalue: hival,
			lowlightingr: this.state.settingsCPS.lowlighting.r,
			lowlightingg: this.state.settingsCPS.lowlighting.g,
			lowlightingb: this.state.settingsCPS.lowlighting.b,
			highlightingr: this.state.settingsCPS.highlighting.r,
			highlightingg: this.state.settingsCPS.highlighting.g,
			highlightingb: this.state.settingsCPS.highlighting.b
		}
		let fetchmasterconst = () => {
			return fetch(this.props.master.lastip + "lightsettings", makePostOptions(lightingpostjson)).then(response => response.text())
		}
		let fetchslaveconst = () => {
			return fetch(this.props.master.lastip + "forward", makePostOptions({
				boardname: this.props.device.boardname,
				postjson: new URLSearchParams(lightingpostjson).toString(),
				post: "1",
				url: "/lightsettings"
			})).then(response => response.text()).then(response => {
				return getForwardData(response).response;
			})
		}
		let fetchfunc;
		if (this.props.index == 0) fetchfunc = fetchmasterconst();
		else fetchfunc = fetchslaveconst();
		timeout(5000, fetchfunc).then((rsp) => {
			if (rsp == "OK") {
				this.lightingposts.current.succeed();
				// save it
				this.props.device.settings.lowlighting = JSON.parse(JSON.stringify(this.state.settingsCPS.lowlighting));
				this.props.device.settings.highlighting = JSON.parse(JSON.stringify(this.state.settingsCPS.highlighting));
				this.props.device.settings.lowvalue = loval;
				this.props.device.settings.highvalue = hival;
				this.props.updateDevice(this.props.device);
			}
			else {
				this.lightingposts.current.fail();
			}
		})
	}

	async delete() {
		console.log("exect");
		// get devices from the master, find our device, if our device is in there delete it, if deletion succeeds then remove from table and update gardens
		this.setState({
			finished: false,
			error: ""
		})
		this.deletionpost.current.begin();
		console.log("eibe");
		try {
			console.log("topfrag")
			let devices = await timeout(4000, fetch(this.props.master.lastip + "devices", getOptions)).then(response => response.text());
			console.log(devices);
			console.log("eibeennenen");
			let deviceslist = devices.split("\n");
			deviceslist.pop();
			console.log(deviceslist);
			for (let i = 0; i < deviceslist.length; i++) {
				let entry = deviceslist[i];
				let entdata = entry.split(" ");

				console.log(entry)

				if (entdata[0].slice(0, -1) == this.props.device.mac) {
					// we gotta delete
					console.log("erioeobg")
					let removal = await timeout(4000, fetch(this.props.master.lastip + "removedevice", makePostOptions({
						boardidx: i
					}))).then(response => response.text());
					console.log(removal);
					if (removal != "OK") {
						throw Error("removal failed");
					}
				}
			}
			// finally remove from devicelists
			let newgardens = JSON.parse(JSON.stringify(this.context.gardens));
			console.log(this.props.idx);
			console.log(JSON.stringify(newgardens[this.props.idx].devices));
			newgardens[this.props.idx].devices.splice(this.props.index - 1, 1);
			console.log(this.props.index - 1);
			console.log(JSON.stringify(newgardens[this.props.idx].devices))
			console.log(JSON.stringify(newgardens))
			console.log("going darl");
			this.props.updateGardens(newgardens);
		}
		catch (e) {
			console.log(e);
			this.deletionpost.current.fail();
		}
	}

	render() {
		let value = this.props.device;
		return (<div key="index" className={"ag-collapse " + (this.props.visible ? "" : "ag-collapsed")}>
			<Popup ref={this.popup} shown={() => {
				this.updateSensorData();
				this.sensorinterval = setInterval(() => {
					console.log("heifhef");
					this.updateSensorData();
				}, 6000);
			}} hidden={() => { clearInterval(this.datainterval) }}>
				<h5>
					<Editable value={value.humanname} update={(val) => {
						this.props.device.humanname = val;
						this.props.updateDevice(this.props.device);
					}}></Editable> <Flash icon="sync" ref={this.datasync}></Flash>
				</h5>
				<div className="row m-0 no-gutters">
					<div className="col-4 text-center">
						<b>Moisture</b>
						<p className="m-0 p-2">
							<i className="material-icons sensoricon font-size-large water-icon" style={{ color: "hsl(200, 100%, " + (value.readings.moisture / 2) + "%)" }}>location_on</i>
						</p>
						<p>{value.readings.moisture}%</p>
					</div>
					<div className="col-4 text-center">
						<b>Sunlight</b>
						<p className="m-0 p-2">
							<i className="material-icons sensoricon font-size-large" style={{ color: "hsl(55, 100%, " + (value.readings.sunlight / 2) + "%)" }}>brightness_{Math.round(value.readings.sunlight / 16.67) + 1}</i>
						</p>
						<p>{value.readings.sunlight}%</p>
					</div>
					<div className="col-4 text-center">
						<b>Temperature</b>
						<p className="m-0 p-2">
							<i className="material-icons sensoricon font-size-large" style={{ color: "hsl(35, 100%, " + Math.min(100, Math.max(0, ((value.readings.temperature + 5) * 1.42857143))) + "%)" }}>wb_sunny</i>
						</p>
						<p>{value.readings.temperature}°C</p>
					</div>
				</div>
				<br />
				<p className={value.watering.watering ? "text-info" : ""}>{value.watering.watering ? "Watering" : "Not watering"}<br /><button className="btn btn-primary" onClick={() => {
					this.startWatering();
				}}>
					Begin watering for 6s
									</button><br /> <button className="btn btn-dark" onClick={() => {
						this.stopWatering();
					}}>
						Stop watering
								</button> <Post ref={this.wateringposts}></Post></p>
				{value.watering.cooldownleft > 0 ? <p>{"Watered recently, won't water automatically for " + millisecondsToStr(value.watering.cooldownleft / 1000)}</p> : null}
				<br />
				<h6>Watering settings</h6>
				<p>Drag the slider to choose a moisture level to maintain for your plants.	</p>
				<p>Automatically water? <input checked={wateringOn(this.state.settingsCPS)} className="d-inline-block" type="checkbox" onChange={
					(e) => {
						if (e.target.checked) {
							this.state.settingsCPS.moistureBegin = 2000 - spread;
							this.state.settingsCPS.moistureEnd = 2000 + spread;
						}
						else {
							this.state.settingsCPS.moistureBegin = 5000;
							this.state.settingsCPS.moistureEnd = 0;
						}
						this.setState({ settingsCPS: this.state.settingsCPS })
					}
				} /></p>
				<div className={wateringOn(this.state.settingsCPS) ? "" : "div-disabled"}>
					<input class="wateringslider" type="range" min="0" max="4095" value={getMoistureSetting(this.state.settingsCPS)} onChange={(e) => {
						this.state.settingsCPS.moistureBegin = Math.max(0, parseInt(e.target.value) - spread);
						this.state.settingsCPS.moistureEnd = Math.min(4096, parseInt(e.target.value) + spread);
						// console.log(Math.min(4096, e.target.value + spread));
						// console.log(e.target.value + spread);
						// console.log(spread);
						// console.log(e.target.value);
						// console.log(e.target.value - spread);
						// console.log(this.state.settingsCPS);
						this.setState({ settingsCPS: this.state.settingsCPS });
					}} />
					<p></p>
					<p><span>100% - fully wet</span><span className="float-right">fully dry - 0%</span></p>
					<p>Current setting: {Math.round(getMoistureSetting(this.state.settingsCPS) / 40.96)}</p>
				</div>
				<button onClick={() => { this.saveMoistureSettings() }} className={"btn btn-primary " + (value.connInfo.connected ? "" : "disabled")}>Save watering settings</button> <Post ref={this.moistureposts}></Post>
				<p></p>
				<h6>Lighting settings</h6>
				{/* we'll just show hue sliders here, and one setting for lights on */}
				<p>Control the colour of the lights on the device based on current sensor readings.</p>
				<p>Turn lights on? <input checked={lightsOn(this.state.settingsCPS)} className="d-inline-block" type="checkbox" onChange={
					(e) => {
						if (e.target.checked) {
							this.state.settingsCPS.lowlighting = {
								r: 255,
								g: 0,
								b: 0
							};
							this.state.settingsCPS.highlighting = {
								r: 255,
								g: 0,
								b: 0
							};
						}
						else {
							this.state.settingsCPS.lowlighting = {
								r: 0,
								g: 0,
								b: 0
							};
							this.state.settingsCPS.highlighting = {
								r: 0,
								g: 0,
								b: 0
							};
						}
						this.setState({ settingsCPS: this.state.settingsCPS })
					}
				} /></p>
				<div className={lightsOn(this.state.settingsCPS) ? "" : "div-disabled"}>
					<p>Change lighting based on: </p>
					<input className="m-1" type="radio" name={value.boardname} value={98} onChange={(e) => { this.changelightingmode(e) }} checked={this.state.settingsCPS.lightingtype == 98} />
					<label>Moisture</label> <br />
					<input className="m-1" type="radio" name={value.boardname} value={97} onChange={(e) => { this.changelightingmode(e) }} checked={this.state.settingsCPS.lightingtype == 97} />
					<label>Sunlight</label> <br />
					<input className="m-1" type="radio" name={value.boardname} value={99} onChange={(e) => { this.changelightingmode(e) }} checked={this.state.settingsCPS.lightingtype == 99} />
					<label>Temperature</label> <br />
					<p>Color when {(() => {
						switch (this.state.settingsCPS.lightingtype) {
							case 98:
								return "dry";
								break;
							case 97:
								return "dark";
								break;
							default:
								return "cold";
								break;
						}
					})()}</p>
					<input className="hueslider" type="range" min="0" max="255" value={rgbtohsv(this.state.settingsCPS.lowlighting).r} onChange={(e) => {
						this.state.settingsCPS.lowlighting = hsvtorgb({
							r: parseInt(e.target.value),
							g: 255,
							b: 255
						});
						this.setState({ settingsCPS: this.state.settingsCPS });
					}} />
					<br />
					<p>Color when {(() => {
						switch (this.state.settingsCPS.lightingtype) {
							case 98:
								return "wet";
								break;
							case 97:
								return "sunny";
								break;
							default:
								return "hot";
								break;
						}
					})()}</p>
					<input className="hueslider" type="range" min="0" max="255" value={rgbtohsv(this.state.settingsCPS.highlighting).r} onChange={(e) => {
						this.state.settingsCPS.highlighting = hsvtorgb({
							r: parseInt(e.target.value),
							g: 255,
							b: 255
						});
						this.setState({ settingsCPS: this.state.settingsCPS });
					}} />
					<br />
				</div>
				<p></p>
				<button onClick={() => { this.saveLightingSettings() }} className={"btn btn-primary " + (value.connInfo.connected ? "" : "disabled")}>Save lighting settings</button> <Post ref={this.lightingposts}></Post>
				<p></p> {/* TODO: advanced settings */}
				<h6>Advanced settings</h6>
				<p>How long (in minutes) should the device wait for a stable reading before beginning to water? <input type="number"
					onChange={(e) => {
						this.state.settingsCPS.checkingTime = e.target.value;
						this.setState({ settingsCPS: this.state.settingsCPS })
					}}
					value={this.state.settingsCPS.checkingTime} />
				</p>
				<p>How long (in minutes) should the device wait in between consecutive waterings? <input type="number"
					onChange={(e) => { this.state.settingsCPS.cooldown = e.target.value; this.setState({ settingsCPS: this.state.settingsCPS }) }} value={this.state.settingsCPS.cooldown} />
				</p>
				<p></p>
				<button onClick={() => { this.saveAdvancedSettings() }} className={"btn btn-primary " + (value.connInfo.connected ? "" : "disabled")}>Save advanced settings</button> <Post ref={this.advancedposts}></Post>
				<p></p>
				<p></p>
				{this.props.index == 0 ? null : (<div>
					<h6>Delete this device</h6>
					<p><button className="btn btn-danger" onClick={() => { this.delete() }}>Delete this device</button> <Post ref={this.deletionpost}></Post></p>
				</div>)}
			</Popup>

			<div className={"row m-0 p-3 align-items-center"} onClick={() => { this.popup.current.showPopup() }}>
				<div className="col-1"></div>
				<div className="col-auto">
					<i className={"material-icons font-size-large " + (value.connInfo.connected ? "text-success" : "text-danger")}>local_florist</i>
				</div>
				<div className="col-auto">
					<span><b>{value.humanname}</b></span>
					<br />
					<span><small className="text-muted">({value.boardname}) {(value.master ? (<span className="text-primary">(main)</span>) : "")}</small></span>
					<p><span className={this.props.device.connInfo.connected ? "text-success" : "text-danger"}>{(this.props.device.connInfo.connected ? "Connected" : "Not found") + " "}</span>
						<Flash ref={this._flashref} icon="sync"></Flash>
					</p>
				</div>
			</div>
		</div >)
	}
}

class Garden extends React.Component {
	static contextType = globalStateContext;
	constructor(props) {
		super(props);
		this.state = {
			visible: false,
			addNewDeviceStage: "connslave",
			boardname: null,
			mac: null,
			settings: {},
			error: "",
			boardip: "",
			finished: false,
			masterpassword: "",
			searchfounds: 0,
			searchnum: 0
		}

		this.setupconnslave = this.setupconnslave.bind(this);
		this.setuprstslave = this.setuprstslave.bind(this);
		this.setupaddtomaster = this.setupaddtomaster.bind(this);

		this.search = this.search.bind(this);
		this._editGardenPopup = React.createRef();
		this._masterflashref = React.createRef();
		this._connslavepost = React.createRef();
		this._rstslavepost = React.createRef();
		this._addtomasterpost = React.createRef();
	}

	componentWillUnmount() {
		clearInterval(this.gardeninterval);
	}

	async search() {
		this.setState({ searchingfound: false, searching: true, searchfounds: 0, searchnum: await ipSpace() }, () => {
			searchIPS((httpip) => {
				console.log(httpip);
				fetch(httpip, getOptions)
					.then(response => response.text())
					.then((response) => {
						// verify the response
						console.log(this.props.garden.master.boardname);
						console.log(response);
						if (this.props.garden.master.boardname == response) {
							let newgardens = this.context.gardens;
							console.log(httpip);
							newgardens[this.props.idx].master.lastip = httpip;
							console.log("FOUND");
							console.log(newgardens);
							this.props.updateGardens(newgardens);
							this.setState({ searchingfound: true });
						}
					})
					.catch(err => console.log(err))
					.finally(() => {
						this.setState((prevState, props) => ({
							searchfounds: prevState.searchfounds + 1
						}));
					});
			});
		});
	}

	async setupconnslave() {
		this._connslavepost.current.begin();
		let p1 = timeout(4000, fetch(ip5 + "settings", getOptions).then(response => response.text())).catch(reason => { console.log("5nocon"); return "NOCONN" });
		let p2 = timeout(4000, fetch(ip6 + "settings", getOptions).then(response => response.text())).catch(reason => { console.log("6nocon"); return "NOCONN" });
		Promise.all([p1, p2]).then((values) => {
			console.log(this.context.gardens)
			console.log(values);
			let localip;
			let success = false;
			if (isSettings(values[0])) {
				this.setState({ boardname: JSON.parse(values[0]).boardname, settings: JSON.parse(values[0]) });
				localip = ip5;
				success = true;
			}
			else if (isSettings(values[1])) {
				this.setState({ boardname: JSON.parse(values[1]).boardname, settings: JSON.parse(values[1]) });
				localip = ip6;
				success = true;
			}
			else {
				this._connslavepost.current.fail();
			}

			if (success) {
				fetch(localip + "mac", getOptions).then(response => response.text()).then(async (macresponse) => {
					// TODO: what if this isn't a mac
					console.log(this.state);
					console.log(this.state.masterpassword);
					let setwifires = await timeout(4000, fetch(localip + "setwifi", makePostOptions({
						ssid: this.props.garden.master.boardname,
						password: this.state.masterpassword
					}))).then(response => response.text());
					console.log({
						ssid: this.props.garden.master.boardname,
						password: this.state.masterpassword
					});
					console.log(setwifires);
					if (setwifires != "OK") throw Error("no ok");
					setTimeout(() => {
						fetch(localip + "connected", getOptions).then(response => response.text()).then((response) => {
							console.log(response);
							if (response == "YES") {
								this._connslavepost.current.reset();
								this.setState({ mac: macresponse, addNewDeviceStage: "rstslave", boardip: localip });
							}
							else {
								this._connslavepost.current.fail();
							}
						})
					}, 4000)
				}).catch((err) => {
					console.log(err);
					this._connslavepost.current.fail();
				})
			}
			console.log(this.context.gardens)
		}).catch((err) => {
			console.log(err);
			this._connslavepost.current.fail();
		});
	}

	setuprstslave() {
		this._rstslavepost.current.begin();
		timeout(5000, fetch(this.state.boardip + "setstate", makePostOptions({
			isSlave: "1"
		}))).finally(() => {
			this._rstslavepost.current.reset();
			this.setState({
				addNewDeviceStage: "addtomaster"
			})
		});
	}

	async setupaddtomaster() {
		this.setState({
			finished: false,
			error: ""
		})
		this._addtomasterpost.current.begin();

		try {
			console.log(this.context.gardens)
			let needtoadd = true;
			let devices = await timeout(15000, fetch(this.context.gardens[this.props.idx].master.lastip + "devices", getOptions)).then(response => response.text());
			let deviceslist = devices.split("\n");
			deviceslist.pop();
			console.log(deviceslist);
			for (let i = 0; i < deviceslist.length; i++) {
				let entry = deviceslist[i];
				let entdata = entry.split(" ");
				// 1 - is it already here?
				if (entdata[0].slice(0, -1) == this.state.mac) {
					if (entdata[1] == this.state.boardname) {
						// it's here
						// succeed
						this.setState({
							finished: true,
							error: "Device already added!"
						})
						needtoadd = false;
					}
					else {
						// it's here but incorrect boardname
						// remove and add it
						let removal = await timeout(15000, fetch(this.context.gardens[this.props.idx].master.lastip + "removedevice", makePostOptions({
							boardidx: i
						}))).then(response => response.text());
						console.log(removal);
						if (removal != "OK") {
							throw Error("removal failed");
						}
					}
				}
			}
			// finally, check if we have space
			if (deviceslist.length > 4) { // TODO: put this in a settings var or smth
				this.setState({
					finished: false,
					error: "Only 4 devices can be connected to this valve."
				})
				throw Error("too many devices");
			}

			if (needtoadd) {
				let addition = await timeout(15000, fetch(this.context.gardens[this.props.idx].master.lastip + "adddevice", makePostOptions({
					boardname: this.state.boardname,
					macaddress: this.state.mac
				}))).then(response => response.text())
				console.log(addition);
				if (addition != "OK") {
					throw Error("addition failed");
				}
			}

			// FINALLY add it to my tables - if we have something with the same mac or same boardname, go overwrite it
			let found = false;
			console.log(this.context.gardens)
			let devicesCP = JSON.parse(JSON.stringify(this.context.gardens[this.props.idx].devices));
			let devicejson = {
				master: false,
				humanname: this.state.boardname,
				boardname: this.state.boardname,
				lastip: null,
				mac: this.state.mac,
				connInfo: {
					connected: false
				},
				settings: this.state.settings,
				readings: {
					moisture: 0,
					temperature: 0,
					sunlight: 0
				},
				watering: {
					"watering": false,
					"reason": '_',
					"cooldownleft": -1
				}
			}
			for (let i = 0; i < devicesCP.length; i++) {
				if (devicesCP[i].mac == this.state.mac) {
					devicesCP[i] = JSON.parse(JSON.stringify(devicejson));
					found = true;
					break;
				}
			}

			if (!found) {
				devicesCP.push(JSON.parse(JSON.stringify(devicejson)));
				console.log("hiuwb");
			}
			this.context.gardens[this.props.idx].devices = devicesCP;
			console.log(this.context.gardens)
			this.props.updateGardens(this.context.gardens);
			this.setState({
				finished: true
			})
			this._addtomasterpost.current.succeed();
		}
		catch (e) {
			console.log(e);
			this._addtomasterpost.current.fail();
		}
	}

	render() {
		return (
			<div className="garden-row" >
				<div className="row m-0 align-items-center p-3">
					<div className="col-auto" onClick={() => { this.updateConnInfo(); this._editGardenPopup.current.showPopup(); }}>
						<i className={"material-icons font-size-large " + (masterConnected(this.props.garden) ? "text-success" : "text-danger")}>grass</i>
					</div>
					<div className="col-auto mr-auto" onClick={() => { this._editGardenPopup.current.showPopup() }}>
						<h6>{this.props.garden.name} <i className="material-icons">edit</i></h6>
						<p className="m-0">{masterConnected(this.props.garden) ? <span className="text-success">Connected</span> : <span className="text-danger">Not found</span>}</p>
					</div>
					<div className="col-auto" onClick={() => { this.setState({ visible: !this.state.visible }) }}>
						<i className={"material-icons font-size-large text-primary dropdown-icon " + (this.state.visible ? "dropdown-icon-rotate" : "")}>keyboard_arrow_right</i>
					</div>
				</div>
				{[this.props.garden.master, ...this.props.garden.devices].map((value, index) => {
					return (
						<Device visible={this.state.visible} device={devicebyindex(this.context.gardens[this.props.idx], index)} key={index} updateGardens={this.props.updateGardens} index={index} master={this.context.gardens[this.props.idx].master}
							updateDevice={(newdevice) => {
								if (index == 0) {
									this.context.gardens[this.props.idx].master = newdevice;
								}
								else {
									this.context.gardens[this.props.idx].devices[index - 1] = newdevice;
								}
								this.props.updateGardens(this.context.gardens);
							}} flashmaster={() => {
								console.log("????")
								this._masterflashref.current.flash();
							}} updateGardens={this.props.updateGardens} idx={this.props.idx}></Device>
					)
				})
				}
				< Popup ref={this._editGardenPopup} shown={() => {
					this.setState({ addNewDeviceStage: "" });
				}}>
					{/* here, we should be able to edit the name of this garden, the wifi username and password of this garden [TODO], and see connection info */}
					< h5 >
						<Editable value={this.props.garden.name} update={(val) => {
							let newgardens = this.context.gardens;
							console.log(this.context);
							console.log(this.props.idx);
							newgardens[this.props.idx].name = val;
							this.props.updateGardens(newgardens);
						}}></Editable>
					</h5 >
					<br />
					<p>Connection status: <span className={this.props.garden.master.connInfo.connected ? "text-success" : "text-danger"}>{(this.props.garden.master.connInfo.connected ? "Connected" : "Not found") + " "}</span>
						<Flash ref={this._masterflashref} icon="sync"></Flash>
					</p>
					<p>If you have changed network (like when you have finished setting this device up) or if you cannot access the device, you can try searching for it.</p>
					<button className="btn btn-primary" onClick={this.search}>Search for the device</button>
					{(this.state.searching ? <p>Searching... ({this.state.searchfounds}/{this.state.searchnum})</p> : null)}
					{(this.state.searchingfound ? <p>Found!</p> : null)}
					<p></p>
					<button className="btn btn-danger" onClick={
						() => {
							let newgardens = this.context.gardens;
							newgardens.splice(this.props.idx, 1);
							this.props.updateGardens(newgardens);
						}
					}>Delete</button>
					<p>Deleting the device will not turn it off - you can still re-add it if it is turned on.</p>
					<p></p>
					<h6>Add a new device to this garden</h6>
					{
						(() => {
							switch (this.state.addNewDeviceStage) {
								case "connslave":
									return (<div>
										<p>Connect to the WiFi network of the device that you want to add</p>
										<p>Your WiFi network should look like "AutoGarden-XXXXXXXX"</p>
										<p>Enter the password of the main board of this garden below</p>
										<p><input onChange={(e) => { this.setState({ masterpassword: e.target.value }) }} /></p>
										<p></p>
										<button className="btn btn-primary" onClick={this.setupconnslave}>Connect</button> <Post ref={this._connslavepost}></Post>
										<p></p>
										<button className="btn btn-primary" onClick={() => { this.setState({ addNewDeviceStage: "rstslave" }) }}>Next</button>
									</div>)
									break;
								case "rstslave":
									return (<div>
										<p>The device may need to restart to set up</p>
										<p></p>
										<button className="btn btn-primary" onClick={this.setuprstslave}>Update</button> <Post ref={this._rstslavepost}></Post>
									</div>)
									break;
								case "addtomaster":
									return (<div>
										<p>Now, connect back to your original network. This garden should be connected.</p>
										<p>Then, press the button below to finish the process.</p>
										<p></p>
										<button className="btn btn-primary" onClick={this.setupaddtomaster}>Finish</button> <Post ref={this._addtomasterpost}></Post>
										<p></p>
										{ this.state.finished ? <p>All done! Close this window to manage your device.</p> : null}
										<p>{this.state.error}</p>
									</div>)
									break;
								default:
									this.setState({
										addNewDeviceStage: "connslave",
										boardname: null,
										mac: null,
										settings: {},
										error: "",
										boardip: "",
										finished: false
									});
									//this._connslavepost.reset();
									//this._rstslavepost.reset();
									//this._addtomasterpost.reset();
									break;
							}
						})()
					}
				</Popup >
			</div >
		)
	}
}

class GardenList extends React.Component {
	static contextType = globalStateContext;
	constructor(props) {
		super(props);
	}

	render() {
		return (
			<div className="main-box">
				{this.context.gardens.map((value, index) => {
					return <Garden updateGardens={this.props.updateGardens} garden={value} key={index} idx={index} />
				})}
			</div>
		)
	}
}

class App extends React.Component {
	constructor(props) {
		super(props);
		var storage = window.localStorage;
		var value = storage.getItem("settings"); // Pass a key name to get its value.
		if (value == null) {
			storage.setItem("settings", JSON.stringify({
				gardens: []
			}));
			value = storage.getItem("settings");
		}
		this._addDevicePopup = React.createRef();
		this.search = this.search.bind(this);
		this.addBoard = this.addBoard.bind(this);
		this.updateGardens = this.updateGardens.bind(this);
		this.setupConnectTo = this.setupConnectTo.bind(this);
		this.setupWifiSettings = this.setupWifiSettings.bind(this);
		this.finishSetup = this.finishSetup.bind(this);
		this.searchEnabledTimeout = null;
		this._wifisetupflash = React.createRef();
		this.setState = this.setState.bind(this);
		this.wificheckinterval;
		this.state = {
			searchEnabled: true,
			searchbegun: false,
			searchfounds: 0,
			searchnum: 0,
			addDeviceDivVisible: false,
			addDeviceSearch: false,
			addDeviceNew: false,
			addDeviceStage: "checkconnectingmaster",

			connecting: false,
			wentwrong: false,
			localip: null,
			sendingwififailed: false,
			wifiname: "",
			wifipwd: "",
			sendingrestart: false,
			complete: false,
			bname: "",
			settingsdoc: {},

			searchResults: [],
			ctx: JSON.parse(value),
			boardbeingadded: null,
			boardbeingaddedErr: false,
			boardbeingaddedSuccess: false,
			retrievedData: {}
		};
	}

	saveState() {
		var storage = window.localStorage;
		storage.setItem("settings", JSON.stringify(this.state.ctx));
		console.log(this.state.ctx);
	}

	updateGardens(newgardens) {
		console.log(newgardens);
		this.state.ctx.gardens = newgardens;	// TODO - this is definitely not how contexts are supposed to be used
		console.log(this.state.ctx);
		this.setState({ ctx: this.state.ctx }, () => {
			console.log(this.state.ctx);
			this.saveState();
		});
	}
	async search() {
		if (this.state.searchEnabled) {
			this.setState({ searchResults: [], searchEnabled: false, searchEnabledTimeout: setTimeout(() => { this.setState({ searchEnabled: true }) }, 5000), searchnum: await ipSpace(), searchbegun: true, searchfounds: 0 });
			searchIPS((httpip) => {
				console.log(httpip);
				fetch(httpip, getOptions)
					.then(response => response.text())
					.then((response) => {
						// verify the response
						if (isBoardname(response)) {
							// valid board
							this.setState((prevState, props) => {
								let searchresults = this.state.searchResults;
								searchresults.push({
									boardname: response,
									ip: httpip
								});
								return {
									searchResults: searchresults
								};
							});
						}
					})
					.catch(err => console.log(err))
					.finally(() => {
						this.setState((prevState, props) => {
							return {
								searchfounds: prevState.searchfounds + 1
							};
						});
					});
			});
		}
	}
	setLED(httpip) {
		timeout(5000, fetch(httpip + "setlighting", makePostOptions({
			r: 255,
			g: 123,
			b: 0,
			time: 7
		})))
			.then(response => response.text())
			.then((response) => {
				console.log(response);
			});
	}
	componentWillUnmount() {
		clearTimeout(this.searchEnabledTimeout);
	}
	async addBoard(idx, httpip) {
		this.setState({ boardbeingadded: idx, retrievedData: {}, boardbeingaddedErr: false, boardbeingaddedSuccess: false });
		// retrieve data
		// i'd like: mac, ip (but we should already have that) and child devices
		// also, force a state change to master
		//let boardnameres, macres, devicesres, stateres
		try {
			let settingsPromise = await timeout(15000, fetch(httpip + "settings", getOptions)).then(response => response.text());
			let macPromise = await timeout(15000, fetch(httpip + "mac", getOptions)).then(response => response.text());
			let devicesPromise = await timeout(15000, fetch(httpip + "devices", getOptions)).then(response => response.text());
			//let statePromise = await fetch(httpip + "setstate", makePostOptions({
			// 	isSlave: 0
			//})).then(response => response.text());

			//Promise.all([boardnamePromise, macPromise, devicesPromise, statePromise]).then((result) => {
			//let results = ["AutoGarden-HEV1OE4M", "30:AE:A4:07:0D:66", "C4:4F:33:75:7A:B1: AutoGarden-ST31LMY3\n", null];
			let results = [settingsPromise, macPromise, devicesPromise];
			console.log(results);
			let masterboard = {
				master: true,
				humanname: JSON.parse(results[0]).boardname,
				boardname: JSON.parse(results[0]).boardname,
				lastip: httpip,
				mac: results[1].toUpperCase(),
				connInfo: {
					connected: true
				},
				settings: JSON.parse(results[0]),
				readings: {
					moisture: 0,
					temperature: 0,
					sunlight: 0
				},
				watering: {
					"watering": false,
					"reason": '_',
					"cooldownleft": -1
				}
			}
			let devicesarr = [];
			let deviceslist = results[2].split("\n");
			let promises = [];
			deviceslist.pop();
			for (let device of deviceslist) {
				let dSplit = device.split(" ");
				let currmac = dSplit[0].slice(0, -1).toUpperCase();
				let dBoardname = dSplit[1];
				// attempt to access the board
				promises.push(
					timeout(15000, fetch(httpip + "forward", makePostOptions({
						boardname: dBoardname,
						postjson: "null",
						post: "0",
						url: "/settings"
					})))
						.then(response => response.text())
						.then((response) => {
							console.log(response);
							// parse the settings json
							devicesarr.push({
								master: false,
								humanname: dBoardname,
								boardname: dBoardname,
								lastip: null,
								mac: currmac,
								connInfo: {
									connected: isSettings(getForwardData(response).response)
								},
								settings: JSON.parse(getForwardData(response).response),
								readings: {
									moisture: 0,
									temperature: 0,
									sunlight: 0
								},
								watering: {
									"watering": false,
									"reason": '_',
									"cooldownleft": -1
								}
							});
						})
						.catch((err) => {
							console.log(err);
							devicesarr.push({
								master: false,
								humanname: dBoardname,
								boardname: dBoardname,
								lastip: null,
								mac: currmac,
								connInfo: {
									connected: false
								},
								settings: {},
								readings: {
									moisture: 0,
									temperature: 0,
									sunlight: 0
								},
								watering: {
									"watering": false,
									"reason": '_',
									"cooldownleft": -1
								}
							});
						})

				);
			}
			Promise.all(promises).then((res) => {
				this.state.ctx.gardens.push({
					name: "New Garden",
					master: masterboard,
					devices: devicesarr
				});
				console.log(JSON.stringify(this.state));
				this.setState({ ctx: this.state.ctx, boardbeingaddedSuccess: true }, () => {
					this.saveState();
				});
			});
		}
		catch (err) {
			console.log(err);
			this.setState({ boardbeingaddedErr: true });
		}
		//}).catch((err) => {
		//	this.setState({ boardbeingaddedErr: true })
		//	console.log(err);
		//})
	}
	setupConnectTo() {
		// try both httpips
		let p1 = timeout(10000, fetch(ip5 + "settings", getOptions).then(response => response.text())).catch(reason => { console.log("5nocon"); return "NOCONN" });
		let p2 = timeout(10000, fetch(ip6 + "settings", getOptions).then(response => response.text())).catch(reason => { console.log("6nocon"); return "NOCONN" });
		Promise.all([p1, p2]).then((values) => {
			console.log(values);
			if (isSettings(values[0])) {
				this.setState({ localip: ip5, connecting: false, bname: JSON.parse(values[0]).boardname, settingsdoc: JSON.parse(values[0]), addDeviceStage: "setupmaster1" });
			}
			else if (isSettings(values[1])) {
				this.setState({ localip: ip6, connecting: false, bname: JSON.parse(values[1]).boardname, settingsdoc: JSON.parse(values[1]), addDeviceStage: "setupmaster1" });
			}
			else {
				this.setState({ wentwrong: true, connecting: false });
			}
		});
		this.setState({
			connecting: true, wentwrong: false
		});
	}
	setupWifiSettings() {
		// send a setwifi
		console.log(this.state);
		fetch(this.state.localip + "setwifi", makePostOptions({
			ssid: this.state.wifiname,
			password: this.state.wifipwd
		})).then(response => response.text()).then(response => {
			this.setState({ sentwifi: true, wifistatusnoconn: true });
			setTimeout(() => {
				fetch(this.state.localip + "connected", getOptions).then(response => response.text()).then((response) => {
					console.log(response);
					if (response == "YES") {
						this.setState({ wifistatusconn: true, wifistatusnoconn: false });
					}
					else {
						this.setState({ wifistatusconn: false, wifistatusnoconn: true });
					}
					this._wifisetupflash.current.flash();
				})
			}, 4000)
		}).catch(reason => {
			console.log(reason);
			this.setState({ sendingwififailed: true });
		});
		this.setState({
			sendingwifi: true,
			sendingwififailed: false,
			sentwifi: false,
			wifistatusconn: false,
			wifistatusnoconn: false
		});
	}
	finishSetup() {
		// send the setstate
		this.setState({ sendingrestart: true });
		fetch(this.state.localip + "setstate", makePostOptions({
			isSlave: "0"
		})).finally(() => {
			this.setState({ complete: true });
			// add the garden
			let gardencp = this.state.ctx.gardens;
			gardencp.push({
				name: "New Garden",
				master: {
					master: true,
					humanname: this.state.bname,
					boardname: this.state.bname,
					lastip: "http://errresolve/",
					mac: "_",
					connInfo: {
						connected: true
					},
					settings: this.state.settingsdoc,
					"readings": {
						"moisture": 0,
						"temperature": 0,
						"sunlight": 0
					},
					watering: {
						"watering": false,
						"reason": '_',
						"cooldownleft": -1
					}
				},
				devices: []
			})
			this.saveState();
			this.setState({ ctx: this.state.ctx });
		});
	}
	render() {
		let statectx = this.state.ctx;
		console.log(this.props.networkinterface);
		statectx["networkinterface"] = this.props.networkinterface;
		let addDeviceSearchDiv = (this.state.addDeviceSearch ? (<div className="container p-2">
			<div className="row">
				<div className="col-12">
					<h6>Search for devices</h6>
					<button className={"btn btn-primary w-100 " + (this.state.searchEnabled ? "" : "disabled")} onClick={this.search}>Search</button>
				</div>
			</div>
			{this.state.searchbegun ? <p>Searching... ({this.state.searchfounds}/{this.state.searchnum})</p> : null}
			{this.state.searchResults.map((value, index) => {
				return (<div className="row">
					<div className="col-auto p-2">
						<p>{value.boardname}</p>
					</div>
					<div className="col-auto ml-auto p-2">
						<button className="btn btn-warning" onClick={() => { this.setLED(value.ip) }}>Turn on LED</button> <button className="btn btn-primary" onClick={() => { this.addBoard(index, value.ip) }}>Add garden</button>
					</div>
					<div className={"col-12 " + (this.state.boardbeingadded == index ? "" : "d-none")}>
						Retrieving data... <br />
						{(this.state.boardbeingaddedErr ? "Something went wrong" : "")}
						{(this.state.boardbeingaddedSuccess ? "Garden was sucessfully added! Close this dialog to manage it." : "")}
					</div>
				</div>)
			})}
		</div>) : null);
		let addDeviceNewDiv = null;
		if (this.state.addDeviceNew) {
			console.log(this.state.addDeviceStage);
			console.log(this.state.addDeviceStage == "restart");
			switch (this.state.addDeviceStage) {
				case "checkconnectingmaster":
					addDeviceNewDiv = (<div className="container p-2">
						<div className="row">
							<div className="col-12">
								<h6>Set up the master</h6>
								<p>Connect to the WiFi network of the valve <b>closest to your router.</b></p>
								<p>Your WiFi network should look like "AutoGarden-XXXXXXXX"</p>
								<button onClick={this.setupConnectTo}>Connect to the board</button>
								{(this.state.connecting ? <p>Connecting...</p> : null)}
								{(this.state.wentwrong ? <p>Couldn't connect.</p> : null)}
							</div>
						</div>
					</div>)
					break;
				case "setupmaster1":
					addDeviceNewDiv = (<div className="container p-2">
						<div className="row">
							<div className="col-12">
								<h6>Set up WiFi</h6>
								<p>Enter the name and password of your router's WiFi</p>
								<p>Name: <input onChange={(e) => { this.setState({ wifiname: e.target.value }); }} /></p> {/* TODO: find the names of ssides */}
								<p>Password: <input onChange={(e) => { this.setState({ wifipwd: e.target.value }); }} /></p>
								<button onClick={this.setupWifiSettings}>Update WiFi</button>
								{(this.state.sendingwifi ? <p>Sending WiFi credentials...</p> : null)}
								{(this.state.sendingwififailed ? <p>Error in sending WiFi credentials!</p> : null)}
								{(this.state.sentwifi ? <p>Sent WiFi credentials...<br />Checking WiFi status</p> : null)}
								<p>{(this.state.wifistatusconn ? <span>Connected! </span> : null)}
									{(this.state.wifistatusnoconn ? <span>Not connected </span> : null)} <Flash icon="sync" ref={this._wifisetupflash}></Flash></p>

								{(true ? <button className="btn btn-primary" onClick={() => { this.setState({ addDeviceStage: "restart" }) }}>Next</button> : null)}
							</div>
						</div>
					</div>)
					break;
				case "restart":
					console.log("here?");
					addDeviceNewDiv = (<div className="container p-2">
						<div className="row">
							<div className="col-12">
								<h6>Finishing up</h6>
								<p>The device may need to restart to finish setting up.</p>
								<button onClick={this.finishSetup}>Finish</button>
								{(this.state.sendingrestart ? <p>Finishing up...</p> : null)}
								{(this.state.complete ? <p>All done! Close this window to manage your valve.</p> : null)}
							</div>
						</div>
					</div>)
					break;
				default:
					this.setState({ addDeviceStage: "checkconnectingmaster" })
					break;
			}
		}
		return (
			<globalStateContext.Provider value={statectx}>
				<div className="main-wrapper">
					<HeaderRow />
					<GardenList updateGardens={this.updateGardens} />
					<div className="buttonsDiv">
						<div onClick={() => { this._addDevicePopup.current.showPopup() }}><i className={"material-icons font-size-large cursor-pointer " + (this.state.searchEnabled ? "" : "text-muted")}>add_circle</i></div>
					</div>
				</div>
				<Popup ref={this._addDevicePopup}>
					<h5>Add a new garden</h5>
					<div className="w-50 d-inline-block p-2">
						<button onClick={() => { this.setState({ addDeviceSearch: true, addDeviceNew: false }); }} className="btn btn-primary w-100">Set up an existing garden</button>
					</div>
					<div className="w-50 d-inline-block p-2">
						<button onClick={() => { this.setState({ addDeviceSearch: false, addDeviceNew: true, addDeviceStage: "checkconnectingmaster" }) }} className="btn btn-primary w-100">Set up a new garden</button>
					</div>
					{addDeviceSearchDiv}
					{addDeviceNewDiv}
				</Popup>
			</globalStateContext.Provider>
		);
	}
}

export default App;
