// Does your browser support web sockets?
if(!window.WebSocket){
	alert("This browser or device does not support WebSockets. You will not be able to use this chat.");
	throw new Error('No websockets :(');
}

// Set some global variables
var browserFocused = true;
var closeIntervalID = null;
var ids = new Object();
var socket = null;
var title = document.title;
var unreadMessages = 0;


if(window.location.protocol === 'https:')
	var protocol = 'wss';
else
	var protocol = 'ws';

if((new Audio).canPlayType('audio/ogg; codecs=vorbis') !== 'no')
	var audio = new Audio(window.location.pathname+'notification.ogg');
else if((new Audio).canPlayType('audio/mpeg') !== 'no')
	var audio = new Audio(window.location.pathname+'notification.mp3');

if(audio)
	audio.volume = 0.5;

// Declare some utility functions
function generateTimeStamp(){
	return (('0'+(new Date).getHours()).slice(-2))+':'+(('0'+(new Date).getMinutes()).slice(-2))+':'+(('0'+(new Date).getSeconds()).slice(-2));
}

function b64encode(str){
	return btoa(str).replace(/=*$/, '').replace(/\+/g,'-').replace(/\//g, '_');
}

function addTabEventListener(tabElem){
	tabElem.addEventListener('click', function(e){
		while(document.querySelector('.selected') !== null)
			document.querySelector('.selected').classList.remove('selected');

		var roomElem = document.querySelector('#rooms > .'+this.classList[0]);

		this.classList.remove('invited');
		this.classList.add('selected');

		if(roomElem === null){
			document.getElementById('rooms').lastElementChild.classList.add('selected');
			return;
		}

		roomElem.classList.add('selected');

		if(this.classList.contains('new_message')){
			this.classList.remove('new_message');

			if(roomElem.querySelectorAll('.options > input[type="checkbox"]')[1].checked)
				roomElem.getElementsByClassName('messages')[0].scrollTop = roomElem.getElementsByClassName('messages')[0].scrollTopMax;
		}

		postEventEvents(this.classList[0]);
	});
}

function postEventEvents(id){
	var roomElem = document.getElementById('rooms').getElementsByClassName(id)[0];
	var tabElem = document.getElementById('tabs').getElementsByClassName(id)[0];
	var messageArea = roomElem.getElementsByClassName('messages')[0];

	if(tabElem !== undefined && !tabElem.classList.contains('selected') && !tabElem.classList.contains('new_message'))
		tabElem.classList.add('new_message');

	var sTop = messageArea.scrollTop;
	var sTopMax = messageArea.scrollTopMax;
	var aScroll = roomElem.querySelectorAll('.options > input[type="checkbox"]')[1].checked;
	var sTmpMax = messageArea.scrollTopMax;

	while(messageArea.children.length > 100)
		messageArea.removeChild(messageArea.firstElementChild);

	if(aScroll)
		messageArea.scrollTop = messageArea.scrollTopMax;
	else if(messageArea.children.length === 100)
		messageArea.scrollTop -= sTmpMax - messageArea.scrollTopMax;
}

// Establish the web socket connection
function establishConnection(){
	if(socket !== null)
		socket.close();

	socket = new WebSocket(protocol+'://'+window.location.hostname+'/ws/chat/');

	socket.onopen = function(){
		document.title = title;

		document.getElementById('rooms').lastElementChild.lastElementChild.removeAttribute('disabled');

		if(closeIntervalID === null)
			return;

		clearInterval(closeIntervalID);

		while(document.getElementsByTagName('style').length)
			document.getElementsByTagName('style')[0].parentNode.removeChild(document.getElementsByTagName('style')[0]);

		for(var id in ids){
			socket.send('join: '+ids[id]['room']+' '+ids[id]['name']);

			document.querySelector('#tabs > .'+id).classList.add(b64encode(ids[id]['room']+'%%'+ids[id]['name']));
			document.querySelector('#tabs > .'+id).classList.remove(id);

			document.querySelector('#rooms > .'+id).classList.add(b64encode(ids[id]['room']+'%%'+ids[id]['name']));
			document.querySelector('#rooms > .'+id).classList.remove(id);

			delete ids[id];
		}

		closeIntervalID = null;
	}

	socket.onmessage = function(msg){
		var data = '', id = '';
		if(msg.data.substring(0, 1) !== "{"){
			id = msg.data.substring(0,16);
			data = JSON.parse(msg.data.slice(16));
		}else
			data = JSON.parse(msg.data);

		switch(data.event){
			case 'chat':
				var div = document.createElement('div');
				var msgContainer = document.getElementById('rooms').getElementsByClassName(id)[0].getElementsByClassName('messages')[0];

				if(decodeURIComponent(data.data)[0] === '>')
					div.classList.add('greentext');

				if(msgContainer.children.length && !msgContainer.lastElementChild.lastElementChild.classList.contains(b64encode(data.user)))
					msgContainer.lastElementChild.classList.add('last');

				div.innerHTML = '<span class="timestamp">['+generateTimeStamp()+']</span> <span class="'+(b64encode(data.user))+'">'+decodeURIComponent(data.user).replace(/</g, '&lt;')+'</span>: '+(decodeURIComponent(data.data).replace(/</g, '&lt;'));
				msgContainer.appendChild(div);

				postEventEvents(id);

				if(!browserFocused)
					document.title = document.title.replace(/ \([0-9]*\)/, '')+' ('+(++unreadMessages)+')';
			break;

			case 'joined':
				document.title = title;

				ids[id] = {
					name: data.user,
					room: data.room
				};

				if(document.querySelector('#tabs > .'+b64encode(data.room+'%%'+data.user)) !== null){
					var tabElem = document.querySelector('#tabs > .'+b64encode(data.room+'%%'+data.user));
					var roomElem = document.querySelector('#rooms > .'+b64encode(data.room+'%%'+data.user));

					tabElem.classList.remove(b64encode(data.room+'%%'+data.user));
					tabElem.className = id+' '+tabElem.className;

					roomElem.classList.remove(b64encode(data.room+'%%'+data.user));
					roomElem.className = id+' '+roomElem.className;

					roomElem.lastElementChild.disabled = '';
					roomElem.getElementsByClassName('messages')[0].innerHTML += '<div style="color:#0F0"><span class="timestamp">['+generateTimeStamp()+']</span> You have been reconnected!</div>';

					roomElem.getElementsByClassName('userlist')[0].innerHTML = '';

					postEventEvents(id);
				}else{
					document.getElementById('name').value = '';
					document.getElementById('room').value = '';
					document.getElementById('room').parentNode.lastElementChild.disabled = '';


					while(document.querySelector('.selected') !== null)
						document.querySelector('.selected').classList.remove('selected');

					var roomElem = document.createElement('div');
					roomElem.classList.add(id);
					roomElem.classList.add('selected');
					roomElem.innerHTML = '<div class="options">'
							+'<button type="button">Invite</button> user <input type="text" /> from room <input type="text" /> to this room.'
							+'<br /><br />'
							+'<button type="button">Clear</button> the chat.'
							+'<br /><br />'
							+'<button type="button">Rename</button> this tab to <input type="text" placeholder="(reset to room name)" />.'
							+'<br /><br />'
							+'<button type="button">Leave</button> this room and close this tab.'
							+'<br /><br />'
							+'<input type="checkbox" /> Notification Sound'
							+'<br /><br />'
							+'<input type="checkbox" checked="checked" /> Auto Scroll'
							+'<br /><br />'
							+'<input type="checkbox" /> Show Notifications in Message Box'
						+'</div>'
						+'<div class="messages">'
						+'</div>'
						+'<div class="info">'
							+'<div class="roomname">'
								+decodeURIComponent(data.room).replace(/</g, '&lt;')
							+'</div>'
							+'<ol class="userlist">'
							+'</ol>'
						+'</div>'
						+'<input type="text" maxlength="350" />';

					document.getElementById('rooms').insertBefore(roomElem, document.getElementById('rooms').firstElementChild);

					roomElem = document.getElementById('rooms').firstElementChild;

					// Invite option
					roomElem.getElementsByClassName('options')[0].getElementsByTagName('button')[0].addEventListener('click', function(){
						socket.send('invite: '+this.parentNode.parentNode.classList[0]+' '+encodeURIComponent(this.parentNode.getElementsByTagName('input')[0].value)+' '+encodeURIComponent(this.parentNode.getElementsByTagName('input')[1].value));

						this.parentNode.getElementsByTagName('input')[0].value = '';
						this.parentNode.getElementsByTagName('input')[1].value = '';
					});

					// Clear option
					roomElem.getElementsByClassName('options')[0].getElementsByTagName('button')[1].addEventListener('click', function(){
						this.parentNode.parentNode.getElementsByClassName('messages')[0].innerHTML = '';
					});

					// Change tab name
					roomElem.getElementsByClassName('options')[0].getElementsByTagName('button')[2].addEventListener('click', function(){
						var name = this.parentNode.getElementsByTagName('input')[2].value;
						var tab = document.querySelector('#tabs .'+this.parentNode.parentNode.classList[0]);

						if(name === '')
							name = this.parentNode.parentNode.getElementsByClassName('roomname')[0].innerHTML.replace(/</g, '&lt;');
						else
							name = decodeURIComponent(name).replace(/</g, '&lt;');

						tab.innerHTML = name+'<span> =</span>';
						this.parentNode.getElementsByTagName('input')[2].value = '';

						tab.firstElementChild.addEventListener('click', function(e){
							var opts = document.querySelector('#rooms > .'+this.parentNode.classList[0]+' .options');

							if(opts.style.display === 'block')
								opts.style.display = '';
							else
								opts.style.display = 'block';
						});
					});

					// Leave room
					roomElem.getElementsByClassName('options')[0].getElementsByTagName('button')[3].addEventListener('click', function(){
						var id = this.parentNode.parentNode.classList[0];

						socket.send('leave: '+id);
						delete ids[id];

						var tab = document.querySelector('#tabs > .'+id);
						var room = document.querySelector('#rooms > .'+id);

						tab.nextElementSibling.classList.add('selected');
						room.nextElementSibling.classList.add('selected');

						tab.nextElementSibling.classList.remove('new_message');
						postEventEvents(room.nextElementSibling.classList[0]);

						tab.parentNode.removeChild(tab);
						room.parentNode.removeChild(room);
					});

					var tabElem = document.createElement('span');
					tabElem.classList.add(id);
					tabElem.classList.add('selected');
					tabElem.innerHTML = (decodeURIComponent(data.room).replace(/</g, '&lt;'))+'<span> =</span>';
					document.getElementById('tabs').insertBefore(tabElem, document.getElementById('tabs').firstElementChild);

					tabElem = document.getElementById('tabs').firstElementChild;
					addTabEventListener(tabElem);

					tabElem.firstElementChild.addEventListener('click', function(e){
						var optionsElem = document.getElementById('rooms').children[Array.prototype.indexOf.call(this.parentNode.parentNode.children, this.parentNode)].getElementsByClassName('options')[0];

						if(optionsElem.style.display === 'block')
							optionsElem.style.display = '';
						else
							optionsElem.style.display = 'block';
					});
				}

				var userlist = roomElem.getElementsByClassName('userlist')[0];
					
				for(var user in data.users){
					var buser = b64encode(user);
					userlist.innerHTML += '<li class="'+buser+'"><span>'+(decodeURIComponent(user).replace(/</g, '&lt;'))+'</span></li>';

					if(document.querySelector('style.'+buser) !== null)
						continue;

					var style = document.createElement('style');
					style.classList.add(buser);
					style.innerHTML = '.'+buser+'>span,.userlist>li+li+li~li.'+buser+',.messages .'+buser+'{';

					if(data.users[user][0] === "#")
						style.innerHTML += 'color:'+data.users[user]+';}';

					document.head.appendChild(style);
				}
			break;

			case 'hi':
				if(!browserFocused)
					document.title = document.title.replace(/\*/, '')+'*';

				var buser = b64encode(data.user);
				var roomElem = document.querySelector('#rooms > .'+id);

				roomElem.getElementsByClassName('userlist')[0].innerHTML += '<li class="'+buser+'"><span>'+(decodeURIComponent(data.user).replace(/</g, '&lt;'))+'</span></li>';

				if(document.querySelector('style.'+buser) === null){
					var style = document.createElement('style');
					style.classList.add(buser);
					style.innerHTML = '.'+buser+'>span,.userlist>li+li+li~li.'+buser+',.messages .'+buser+'{';
					
					if(data.color[0] === "#")
						style.innerHTML += 'color:'+data.color+';}';

					document.head.appendChild(style);
				}

				if(roomElem.querySelectorAll('.options > input[type="checkbox"]')[2].checked){
					if(roomElem.getElementsByClassName('messages')[0].lastElementChild !== null)
						roomElem.getElementsByClassName('messages')[0].lastElementChild.classList.add('last');

					roomElem.getElementsByClassName('messages')[0].innerHTML += '<div style="color:#0F0"><span class="timestamp">['+generateTimeStamp()+']</span> '+decodeURIComponent(data.user).replace(/</g, '&lt;')+' has arrived!</div>';
				}

				postEventEvents(id);
			break;

			case 'invite':
				if(!browserFocused)
					document.title = document.title.replace(/\+/, '')+'+';

				document.getElementById('room').value = decodeURIComponent(data.invite);
				document.getElementById('tabs').lastElementChild.classList.add('invited');

				var roomElems = document.getElementById('rooms').children;

				for(var i=0; i<roomElems.length - 1; i++)
					if(roomElems[i].querySelectorAll('.options > input[type="checkbox"]')[2].checked){
						if(roomElems[i].getElementsByClassName('messages')[0].lastElementChild !== null)
							roomElems[i].getElementsByClassName('messages')[0].lastElementChild.classList.add('last');

						roomElems[i].getElementsByClassName('messages')[0].innerHTML += '<div style="color:#FF0"><span class="timestamp">['+generateTimeStamp()+']</span> You have been invited to a new room! Click the yellow + tab to join!</div>';
					}
			break;

			case 'bye':
				if(!browserFocused)
					document.title = document.title.replace(/\*/, '')+'*';

				var roomElem = document.querySelector('#rooms > .'+id);

				roomElem.getElementsByClassName('userlist')[0].removeChild(roomElem.getElementsByClassName('userlist')[0].getElementsByClassName(b64encode(data.user))[0]);

				if(roomElem.querySelectorAll('.options > input[type="checkbox"]')[2].checked){
					if(roomElem.getElementsByClassName('messages')[0].lastElementChild !== null)
						roomElem.getElementsByClassName('messages')[0].lastElementChild.classList.add('last');

					roomElem.getElementsByClassName('messages')[0].innerHTML += '<div style="color:#F00"><span class="timestamp">['+generateTimeStamp()+']</span> '+decodeURIComponent(data.user).replace(/</g, '&lt;')+' has left!</div>';
				}

				postEventEvents(id);
			break;

			case 'error':
				document.title = title;
				document.getElementById('room').parentNode.lastElementChild.disabled = '';

				alert(data.msg);
			break;

			default:
				return;
		}

		if(audio && !browserFocused && id !== '' && document.querySelectorAll('#rooms > .'+id+' > .options > input[type="checkbox"]')[0].checked)
			audio.play();
	}

	socket.onclose = function(){
		document.title = 'Disconnected!';

		document.getElementById('room').parentNode.lastElementChild.disabled = 'disabled';

		var rooms = document.getElementById('rooms').children;
		for(var i=0; i<rooms.length - 1; i++){
			rooms[i].lastElementChild.disabled = "disabled";

			if(rooms[i].getElementsByClassName('messages')[0].lastElementChild !== null)
				rooms[i].getElementsByClassName('messages')[0].lastElementChild.classList.add('last');

			rooms[i].getElementsByClassName('messages')[0].innerHTML += '<div class="last" style="color:#F00"><span class="timestamp">['+generateTimeStamp()+']</span> You have been disconnected... We will be reconnecting you shortly</div>';
			postEventEvents(rooms[i].classList[0]);
		}

		if(closeIntervalID === null)
			closeIntervalID = setInterval(function(){
				document.title = 'Reconnecting...';

				if(closeIntervalID !== null)
					establishConnection();
			}, 5000);
	}
}

establishConnection();

// Now to set some window.onX events
window.onfocus = function(){
	browserFocused = true;
	unreadMessages = 0;

	if(document.title !== title && socket.readyState === 1){
		document.title = title;
	}
}

window.onblur = function(){
	browserFocused = false;
}


// Finally, some stuff to do on page load. Since this script is loaded with async set, the script won't load until the page is already done loading
addTabEventListener(document.getElementById('tabs').lastElementChild);

document.getElementById('rooms').addEventListener('submit', function(e){
	e.preventDefault();

	if(e.explicitOriginalTarget.parentNode === document.getElementById('rooms').lastElementChild){
		socket.send('join: '+encodeURIComponent(document.getElementById('room').value)+' '+encodeURIComponent(document.getElementById('name').value));
		document.title = 'Connecting...';
		document.getElementById('room').parentNode.lastElementChild.disabled = 'disabled';
		return;
	}

	var input = e.explicitOriginalTarget;
	if(input.value === '' || !input.parentNode.classList.contains('selected'))
		return;

	socket.send('chat: '+input.parentNode.classList[0]+' '+encodeURIComponent(input.value));
	input.value = '';
});

if(window.location.hash.length > 1)
	document.getElementById('room').value = decodeURIComponent(window.location.hash.substr(1)).replace(/&lt;/g, "<");
