//var socket = new WebSocket('ws://'+window.location.host+':8080');
var socket = new WebSocket('ws://127.0.0.1:8080');

function tabClickFunc(e){
	if(this.classList.contains('selected'))
		return;

	document.getElementsByClassName('selected')[0].classList.remove('selected');
	document.getElementsByClassName('selected')[0].classList.remove('selected');

	var tabs = document.getElementById('tabs').children;

	for(var i=0; i<tabs.length; i++)
		if(tabs[i] === this){
			document.getElementById('rooms').children[i].classList.add('selected');
			this.classList.add('selected');

			return;
		}

	document.getElementById('tabs').firstElementChild.classList.add('selected');
	document.getElementById('rooms').firstElementChild.classList.add('selected');
}

function attrEncode(str){
	return btoa(encodeURIComponent(str));
}

socket.onopen = function(e){
	console.log("opened!", e);

	document.querySelector('button[name="join"]').removeAttribute('disabled');
}

socket.onclose = function(e){
	console.log("closed!", e);

	document.querySelector('button[name="join"]').setAttribute('disabled', 'disabled');
}

socket.onmessage = function(e){
	var datas = e.data.split("\u0000");

	console.log(datas, e);

	switch(datas[0]){
		case 'chat':
			var div = document.createElement('div');
			var nameSpan = document.createElement('span');
			var message = document.createTextNode(': '+datas[3]);

			div.innerHTML = '<span class="timestamp">['+((new Date).toTimeString().split(' ')[0])+']</span>';

			nameSpan.textContent = datas[1];
			nameSpan.className = attrEncode(datas[1]+'\0'+datas[2]);

			div.appendChild(nameSpan);
			div.appendChild(message);

			document.querySelector('[data-name="'+attrEncode(datas[4]+'\0'+datas[5])+'"]').parentNode.getElementsByClassName('messages')[0].appendChild(div);
		break;

		case 'hi':
			document.getElementById('dynamic_style_sheet').sheet.insertRule('.'+(attrEncode(datas[1]+'\0'+datas[2]).replace(/=/g, '\\=').replace(/\+/g, '\\+'))+'{color:#'+datas[3]+';}');

			var span = document.createElement('span');
			span.textContent = datas[1];
			span.className = attrEncode(datas[1]+'\0'+datas[2]);

			var li = document.createElement('li');
			li.appendChild(span);

			document.querySelector('[data-name="'+attrEncode(datas[4]+'\0'+datas[5])+'"]').parentNode.getElementsByClassName('userlist')[0].appendChild(li);
		break;

		case 'bye':
			var li = document.querySelector('[data-name="'+attrEncode(datas[3]+'\0'+datas[4])+'"]').parentNode.querySelector('.userlist .'+(attrEncode(datas[1]+'\0'+datas[2]).replace(/=/g, '\\=').replace(/\+/g, '\\+'))).parentNode;
			li.parentNode.removeChild(li);
		break;

		case "joined":
			document.getElementById('name').value = '';
			document.getElementById('room').value = '';

			var tab_span = document.createElement('span');
			var room_div = document.createElement('div');

			tab_span.textContent = datas[1];
			room_div.innerHTML = '<div class="messages">'+
				'</div>'+
				'<div class="info_box">'+
					'<div class="room_name"></div>'+
					'<ol class="userlist"></ol>'+
				'</div>'+
				'<input type="text" />';

			document.getElementsByClassName('selected')[0].classList.remove('selected');
			document.getElementsByClassName('selected')[0].classList.remove('selected');

			document.getElementById('tabs').appendChild(tab_span);
			tab_span = document.getElementById('tabs').lastElementChild;
			tab_span.className = 'selected';

			document.getElementById('rooms').appendChild(room_div);
			room_div = document.getElementById('rooms').lastElementChild;
			room_div.className = 'selected';
			room_div.lastElementChild.dataset['name'] = attrEncode(datas[1]+'\0'+datas[2]);

			tab_span.addEventListener('click', tabClickFunc);

			room_div.getElementsByClassName('room_name')[0].textContent = datas[1];
		break;

		case 'invite':
			console.log('you got an invite. Woo', e, datas);
		break;

		case "error":
			alert(datas[1]);
		break;

		default:
	}
}

window.onload = function(){
	document.getElementById('rooms').addEventListener('submit', function(e){
		e.preventDefault();

		if(document.querySelector('#tabs > .selected.d_fault') !== null){
			console.log('joining');
			socket.send("join\0"+document.getElementById('room').value + "\0" + document.getElementById('name').value+'\0');
		}else{
			var input = document.querySelector('#rooms > .selected input');

			if(!input.value.length)
				return;

			socket.send('chat\0'+decodeURIComponent(atob(input.dataset['name']))+'\0'+input.value+'\0');
			input.value = '';
		}
	});

	document.getElementById('tabs').firstElementChild.addEventListener('click', tabClickFunc);
}
