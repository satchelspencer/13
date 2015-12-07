var websocket = function(host, protocol){
	if(!window.WebSocket) return false; //websockets not supported
	var socket = new WebSocket(host);
	/* object with command listeners */
	var events = {};
	var error = function(){};
	var ready = function(){};
	/* public api */
	var api = {
		send : function(command, body){
			socket.send(protocol+':'+command+':'+encodeURIComponent(body));
			return api;
		},
		on : function(command, callback){
			events[command] = callback;
			return api;
		},
		off : function(command){
			delete events[command];
			return api;
		},
		error : function(callback){
			error = callback;
			return api;
		},
		ready : function(callback){
			ready = callback;
			return api;
		},
	}

	socket.onopen = function(evt) {
		/* establish credentials with server */
		socket.send('global:connect:'+document.cookie.split('=')[1]);
	};
	socket.onmessage = function(evt){
		var chunks = evt.data.split(':');
		/* listen for response from initial connect */
		if(chunks[0] == 'global' && chunks[1] == 'connect'){
			/* if message is not 'success' callback with error */
			if(chunks[2] != 'success') error(chunks[2]);
			else ready();
		}else if(chunks[0] == protocol && events[chunks[1]]) events[chunks[1]](chunks[2]);
		/* ^^ otherwise check if the message is for the protocol and being listened for */
	};
	return api;
}