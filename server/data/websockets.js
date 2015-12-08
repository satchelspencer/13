var websocket = function(host){
	if(!window.WebSocket) return false; //websockets not supported
	var socket = new WebSocket(host);
	/* object with command listeners */
	var events = {};
	var error = function(){};

	socket.onopen = function(evt) {
		/* establish credentials with server */
		socket.send('global:connect:'+document.cookie.split('=')[1]);
	};
	socket.onmessage = function(evt){
		var chunks = evt.data.split(':');
		/* listen for response from initial connect */
		if(chunks[0] == 'global' && chunks[1] == 'error') error(chunks[2]);
		else if(events[chunks[0]] && events[chunks[0]][chunks[1]]) events[chunks[0]][chunks[1]](chunks[2]);
		/* ^^ otherwise check if the message is for the protocol and being listened for */
	};
	socket.onerror = function(evt){
		error(evt.data);
	};
	socket.onclose = function(evt){
		error(evt.data);
	};

	return {
		in : function(protocol){
			events[protocol] = events[protocol]||{};
			/* public api */
			var api = {
				send : function(command, body){
					socket.send(protocol+':'+command+':'+encodeURIComponent(body));
					return api;
				},
				on : function(command, callback){
					events[protocol][command] = callback;
					return api;
				},
				off : function(command){
					delete events[protocol][command];
					return api;
				}
			}
			return api;
		},
		leave : function(){
			socket.send('global:disconnect:'+document.cookie.split('=')[1]);
			socket.close();
		},
		error : function(callback){
			error = callback;
		}
	}
}