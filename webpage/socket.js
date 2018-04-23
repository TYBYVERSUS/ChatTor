//var socket = new WebSocket('ws://'+window.location.host+':8080');
var socket = new WebSocket('ws://127.0.0.1:8080');

var invites = new Set();
var browserFocused = true;
var title = document.title;
var unreadMessages = 0;

if((new Audio).canPlayType('audio/ogg; codecs=vorbis') !== 'no')
	var audio = new Audio(window.location.pathname.substr(0, window.location.pathname.lastIndexOf('/')+1)+'notification.ogg');
else if((new Audio).canPlayType('audio/mpeg') !== 'no')
	var audio = new Audio(window.location.pathname.substr(0, window.location.pathname.lastIndexOf('/')+1)+'notification.mp3');

if(audio)
	audio.volume = 0.5;

window.onclick = function(){
	var elem;
	while((elem = document.getElementsByClassName('open')[0]) !== undefined)
		elem.classList.remove('open');
}

window.onfocus = function(){
	browserFocused = true;
	unreadMessages = 0;

	if(document.title !== title && socket.readyState === 1)
		document.title = title;
}

window.onblur = function(){
	browserFocused = false;
}

function tabClickFunc(e){
	if(this.classList.contains('selected'))
		return;

	document.getElementsByClassName('selected')[0].classList.remove('selected');
	document.getElementsByClassName('selected')[0].classList.remove('selected');

	var tabs = document.getElementById('tabs').children;
	var tab = document.getElementById('tabs').firstElementChild;
	var room = document.getElementById('rooms').firstElementChild;

	for(var i=0; i<tabs.length; i++)
		if(tabs[i] === this){
			tab = this;
			room = document.getElementById('rooms').children[i];
			break;
		}

	tab.classList.remove('new_message');

	tab.classList.add('selected');
	room.classList.add('selected');
}

function attrEncode(str){
	return btoa(encodeURIComponent(str));
}

function cleanupMessages(div){
	var sH = div.scrollHeight;

	while(div.children.length > 100)
		div.removeChild(div.firstElementChild);

	var options = div.parentNode.getElementsByClassName('options')[0].children;

	if(options[8].firstElementChild.checked)
		div.scrollTop = sH;
	else if(div.children.length === 100)
		div.scrollTop -= sH - div.scrollHeight;

	if(audio && options[10].firstElementChild.checked && (!browserFocused || options[12].firstElementChild.checked))
		audio.play();
}

socket.onopen = function(e){
	document.querySelector('button[name="join"]').removeAttribute('disabled');
	document.querySelector('button[name="join"]').textContent = 'Join!';
}

socket.onclose = function(e){
	document.querySelector('button[name="join"]').setAttribute('disabled', 'disabled');
	document.querySelector('button[name="join"]').textContent = 'Reconnecting...';
}

socket.onmessage = function(e){
	var datas = e.data.split("\u0000");

	switch(datas[0]){
		case 'chat':
			var container = document.querySelector('[data-name="'+attrEncode(datas[4]+'\0'+datas[5])+'"]').parentNode;
			var messages = container.getElementsByClassName('messages')[0];
			var tab = document.getElementById('tabs').children[[...container.parentNode.children].indexOf(container)];

			var div = document.createElement('div');
			var nameSpan = document.createElement('span');
			var message = document.createTextNode(': '+datas[3]);

			div.innerHTML = '<span class="timestamp">['+((new Date).toTimeString().split(' ')[0])+']</span>';

			nameSpan.textContent = datas[1];
			nameSpan.className = attrEncode(datas[1]+'\0'+datas[2]);

			if(container.firstElementChild.children.length !== 0 && nameSpan.className !== container.firstElementChild.lastElementChild.children[1].className)
				div.className = 'new_speaker'

			div.appendChild(nameSpan);
			div.appendChild(message);

			messages.appendChild(div);
			cleanupMessages(messages);

			if(!tab.classList.contains('selected'))
				tab.classList.add('new_message');
		break;

		case 'hi':
			document.getElementById('dynamic_style_sheet').sheet.insertRule('.'+(attrEncode(datas[1]+'\0'+datas[2]).replace(/=/g, '\\=').replace(/\+/g, '\\+'))+'{color:#'+datas[3]+';}');

			var div = document.createElement('div');
			div.textContent = datas[1];
			div.className = attrEncode(datas[1]+'\0'+datas[2]);

			var li = document.createElement('li');
			li.appendChild(div);

			div = document.createElement('div');
			div.innerHTML = '<div>Tripcode Here</div><select><option value="">Invite To</option></select>';
			li.appendChild(div);

			div.lastElementChild.addEventListener('click', function(e){
				this.innerHTML = '<option value="">Invite To</option>';

				var elems = document.querySelectorAll('#rooms > div:not(.d_fault) .room_name');
				var s = new Set();

				for(var i=0; i<elems.length; i++){
					if(elems[i].textContent === this.parentNode.parentNode.parentNode.previousElementSibling.previousElementSibling.textContent || s.has(elems[i].textContent))
						continue;

					s.add(elems[i].textContent);
					var option = document.createElement('option');
					option.textContent += elems[i].textContent;
					this.appendChild(option);
				}
			});

			div.lastElementChild.addEventListener('input', function(e){
				var room = this.parentNode.parentNode.parentNode.parentNode.firstElementChild.textContent;
				var to_room = this.selectedOptions[0].value;
				var name = this.parentNode.previousElementSibling.textContent;

				if(to_room === '')
					return;

				if(confirm('Are you sure you want to invite user "'+name+'" from room "'+room+'" to room "'+to_room+'"?'))
					socket.send('invite\0'+room+'\0'+name+'\0'+to_room);
			});

			document.querySelector('[data-name="'+attrEncode(datas[4]+'\0'+datas[5])+'"]').parentNode.getElementsByClassName('userlist')[0].appendChild(li);
			li.addEventListener('click', function(e){
				this.classList.add('open');
				e.stopPropagation();
			});
		break;

		case 'leave':
			var div = document.querySelector('[data-name="'+attrEncode(datas[1]+'\0'+datas[2])+'"]').parentNode;
			var index = [...div.parentNode.children].indexOf(div);

			var tabs = document.getElementById('tabs');
			var rooms = div.parentNode;

			tabs.removeChild(tabs.children[index]);
			rooms.removeChild(div);

			tabs.children[0].classList.add('selected');
			rooms.children[0].classList.add('selected');
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
					'<div class="option_arrow">V'+
						'<div class="options">'+
							'<button>Clear Chat</button><button>Leave Room</button><br /><br />'+
							'<input type="text" placeholder="(reset)" /><button>Name Tab</button><br /><br />'+
							'<label><input type="checkbox" checked="checked" />Auto Scroll</label><br />'+
							'<label><input type="checkbox" />Message Sound</label><br />'+
							'<label><input type="checkbox" />Sound Always</label><br />'+
							'<label><input type="checkbox" />Notification Messages</label>'+
						'</div>'+
					'</div>'+
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

			room_div.getElementsByClassName('option_arrow')[0].addEventListener('click', function(e){
				e.stopPropagation();
				this.classList.add('open');
			});

			var options = room_div.getElementsByClassName('options')[0];
			options.children[0].addEventListener('click', function(){ room_div.getElementsByClassName('messages')[0].innerHTML = ''; });
			options.children[1].addEventListener('click', function(){ socket.send('leave\0'+decodeURIComponent(atob(room_div.lastElementChild.dataset['name']))); });

			options.children[5].addEventListener('click', function(){
				if(options.children[4].value === '')
					document.getElementById('tabs').children[[...room_div.parentNode.children].indexOf(room_div)].textContent = room_div.getElementsByClassName('room_name')[0].textContent;

				else{
					document.getElementById('tabs').children[[...room_div.parentNode.children].indexOf(room_div)].textContent = options.children[4].value;
					options.children[4].value = '';
				}
			});

			room_div.getElementsByClassName('room_name')[0].textContent = datas[1];
		break;

		case 'invite':
			document.getElementById('tabs').firstElementChild.classList.add('invited');
			invites.add(datas[1]);

			if(invites.size){
				document.querySelector('[name="show_invite"]').style.display = 'inline';

				if(invites.size > 1){
					document.querySelector('[name="clear_invites"]').style.display = 'inline';
					document.querySelector('[name="clear_invites"]').textContent = 'Clear '+invites.size+' Invites';
				}
			}
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

		if(document.querySelector('#tabs > .selected.d_fault') !== null)
			socket.send("join\0"+document.getElementById('room').value + "\0" + document.getElementById('name').value+'\0');

		else{
			var input = document.querySelector('#rooms > .selected input[type="text"]:last-child');
			if(!input.value.length)
				return;

			socket.send('chat\0'+decodeURIComponent(atob(input.dataset['name']))+'\0'+input.value+'\0');
			input.value = '';
		}
	});

	document.getElementById('tabs').firstElementChild.addEventListener('click', tabClickFunc);
	document.querySelector('[name="show_invite"]').addEventListener('click', function(e){
		var invite = [...invites][0];
		invites.delete(invite);

		document.getElementById('room').value = invite;

		if(!invites.size){
			this.style.display = 'none';
			document.getElementById('tabs').firstElementChild.classList.remove('invited');

		}else{
			if(invites.size === 1)
				this.nextElementSibling.style.display = 'none';
			else
				this.nextElementSibling.textContent = 'Clear '+invites.size+' Invites';
		}
	});

	document.querySelector('[name="clear_invites"]').addEventListener('click', function(e){
		invites.clear();

		document.getElementById('tabs').firstElementChild.classList.remove('invited');

		this.style.display = 'none';
		this.previousElementSibling.style.display = 'none';
	});
}
