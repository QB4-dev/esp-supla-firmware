function tab(e, id) {
  let tab = document.getElementsByClassName("tab");
  for (var i = 0; i < tab.length; i++)
    tab[i].style.display = "none";

  document.getElementById(id).style.display = "block";
  e.currentTarget.className += " active";
}

function reset(e) {
    if (confirm("Do you want to reset the board?") == true) {
	    fetch(esp_url+"/settings?action=restart");
	    alert("Board reset - disconnected");
    }
}

function suplaStats(){
	fetch(esp_url+"/supla")
	.then(r => r.json())
	.then(js => {
		const div = document.getElementById('device');

		function secToTime(e){
			const h = Math.floor(e / 3600).toString().padStart(2,'0'),
			m = Math.floor(e % 3600 / 60).toString().padStart(2,'0'),
			s = Math.floor(e % 60).toString().padStart(2,'0');
			return h + ':' + m + ':' + s;
		}

		let stats = `<small>
		Device: ${js.data.name}<br>
		software: ${js.data.software_ver}<br>
		state: ${js.data.state}<br>
		GUID: ${js.data.guid}<br>
		uptime: ${secToTime(js.data.uptime)}<br>
		connection uptime: ${secToTime(js.data.connection_uptime)}
		</small>`;
		document.getElementById("default").innerHTML = js.data.name;
		div.innerHTML = stats;
		return setTimeout(suplaStats, 2000);
	});
}

function netStats(){
	fetch(esp_url+"/wifi")
	.then(r => r.json())
	.then(js => {
		const div = document.getElementById('net');
		let sta = js.data.sta;

		let stats = `
			<small>
				Status: ${sta.status}<br>`;
		if(sta.connection)
			stats += `
				Network: ${sta.connection.ssid}<br>
				authmode: ${sta.connection.authmode}<br>
				RSSI: ${sta.connection.rssi} dB<br>`;

		stats +=`
			IP addr: ${sta.ip_info.ip}<br>
			netmask: ${sta.ip_info.netmask}<br>
			gateway: ${sta.ip_info.gw}
		</small>`;

		div.innerHTML = stats;
		return setTimeout(netStats, 2000);
	});
}

function getWiFiConfigForm(){
	fetch(esp_url+"/wifi?action=get_config")
	.then(r => r.json())
	.then(js => {
		const div = document.getElementById('wifi');

		let html = `
		<form id="wifi-form" method="post">
			SSID<br>
			<input type="text" name="ssid" value="${js.data.sta.ssid}" style="max-width:300px"><br>
			Password<br>
			<input type="password" name="passwd" style="max-width:300px"><br>
			<input type="submit" value="submit">
		</form>`;

		div.innerHTML = html;
		const form = document.getElementById('wifi-form');
		form.onsubmit = function(e){
			e.preventDefault();
			let data = decodeURIComponent(new URLSearchParams(new FormData(this)));

			fetch(esp_url+"/wifi?action=connect",
			{
				method: "POST",
				body: data
			})
			.then(r => r.json())
			.then(js => { alert("WiFi config changed"); });
		};
	})
}

function getSuplaConfigForm(){
	fetch(esp_url+"/supla?action=get_config")
	.then(r => r.json())
	.then(js => {
		const div = document.getElementById('supla');

		let html = `
		<form id="supla-form" method="post">
			Email<br>
			<input type="text" name="email" value="${js.data.email}" style="max-width:300px"><br>
			Server<br>
			<input type="text" name="server" value="${js.data.server}" style="max-width:300px"><br>
            ${js.data.hasOwnProperty('ssl')?`<input type="checkbox" name="ssl" ${js.data.ssl?"checked":""}>SSL<br>`:""}
            <input type="submit" value="submit">
		</form>`;

		div.innerHTML = html;
		const form = document.getElementById('supla-form');
		form.onsubmit = function(e){
			e.preventDefault();
			let data = decodeURIComponent(new URLSearchParams(new FormData(this)));

			fetch(esp_url+"/supla?action=set_config",
			{
				method: "POST",
				body: data
			})
			.then(r => r.json())
			.then(js => { alert("SUPLA config changed"); });
		};

	})
}

function getSettingsForm(){
	fetch(esp_url+"/settings")
	.then(r => r.json())
	.then(js => {
		const div = document.getElementById('brd');

		let html = `<form id="brd-form" method="post">`;
		js.data.groups.forEach((gr, i) => {
			html+=`${gr.label}:<br>`;
			html+='<small>';
			gr.settings.forEach((item, i) => {
				switch (item.type) {
					case "BOOL":
						html+=`<input type="checkbox" name="${gr.id}:${item.id}" ${item.val?"checked":''}>${item.label}<br>`;
						break;
					case "NUM":
						html+=`<span class="label-inline">${item.label}</span><input type="number" name="${gr.id}:${item.id}" min=${item.min} max=${item.max} value=${item.val} style="width:150px"><br>`;
						break;
					case "ONEOF":
						html+=`<span class="label-inline">${item.label}</span><select name="${gr.id}:${item.id}" style="width:160px">`;
						item.options.forEach((txt, i) => { html+=` <option value=${i} ${item.val==i?"selected":''}>${txt}</option>`;});
						html+=`</select><br>`;
						break;
					case "TIME":
						let time = String(item.hh).padStart(2,'0')+":"+String(item.mm).padStart(2,'0');
						html+=`<span class="label-inline">${item.label}</span><input type="time" name="${gr.id}:${item.id}" min="00:00" max="23:59" value="${time}" style="width:150px"/><br>`;
						break;
					default:
						break;
				}
			});
			html+=`</small>`;
		});
		html+=`<input type="submit" value="submit"></form>`;
		html+=`<button onclick='reset()'>RESTART</button>`;

		div.innerHTML = html;
		const form = document.getElementById('brd-form');
		form.onsubmit = function(e){
			e.preventDefault();
			let data = decodeURIComponent(new URLSearchParams(new FormData(this)));
			fetch(esp_url+"/settings?action=set",
			{
				method: "POST",
				body: data
			})
			.then(r => r.json())
			.then(js => { alert("board config changed"); });
		};
	})
}

function appInfo(){
	fetch(esp_url+"/info")
	.then(r => r.json())
	.then(js => {
		const div = document.getElementById('app');

		let info = `
			<h6>Firmware:</h6>
			<small>project: ${js.project_name}</small><br>
			<small>version: ${js.version}</small><br>
			<small>idf_ver: ${js.idf_ver}</small><br>
			<small>bulid date: ${js.date}</small><br>
			<small>bulid time: ${js.time}</small><br>`;

		div.innerHTML = info;
	});
}

function uploadFile(event){
	event.preventDefault();
	var xhttp = new XMLHttpRequest();
	var data = new FormData(event.target);

	xhttp.onload = function() {
		if(this.status == 200){
			var js = JSON.parse(this.responseText);
			alert((js.result==='ESP_OK')?('Update success:\n\n'+js.bytes_uploaded +'b uploaded'):('Update error'));
		} else {
			alert('Request Error');
		}
		event.target.style.display = 'block';
		loadbar.style.display = 'none';
	};

	xhttp.ontimeout = function(e) {
		info.innerHTML='Error: request timed out';
		event.target.style.display = 'block';
		loadbar.style.display = 'none';
	};

	xhttp.timeout = 120000;
	xhttp.open("POST",event.target.action, true);
	loadbar.style.display = 'block';
	event.target.style.display = 'none';
	xhttp.send(data);
}
