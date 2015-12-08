/**
 * websocket class to handle communication with esp8266
 * @param host url of websocket to connect to, in our case 'ws://192.168.1.1:81'
 * @return connection object  
*/
var websocket = function(host){
	if(!window.WebSocket) return false; //websockets not supported
	var socket = new WebSocket(host);
	/* object with command listeners */
	var events = {};
	var error = function(){};
​
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
​
	/**
	 * connection object, contains methods for manipulating websocket connection
	*/
	return {
		/**
		 * create a new access object in a particular protocol context
		 * @param protocol name of protocol i.e 'chat' or 'global'
		 * @return websocket object to send and listen for events
		*/
		in : function(protocol){
			events[protocol] = events[protocol]||{};
			/* websocket object */
			var api = {
				/**
				 * send a message over the protocol
				 * @param command name of command to be executed on server
				 * @param body content of command to be sent
				*/
				send : function(command, body){
					socket.send(protocol+':'+command+':'+encodeURIComponent(body));
					return api;
				},
				/**
				 * listen for an incoming message in the protocol
				 * @param command name of command to be listened for
				 * @param callback function to be triggered on command recieved. passes as only argument the body
				*/
				on : function(command, callback){
					events[protocol][command] = callback;
					return api;
				},
				/**
				 * disable and free memory for event callback
				 * @param command name of command for event to be disabled
				*/
				off : function(command){
					delete events[protocol][command];
					return api;
				}
			}
			return api;
		},
		/**
		 * disconnect from the websocket, sends diconnection command to server before close
		*/
		leave : function(){
			socket.send('global:disconnect:'+document.cookie.split('=')[1]);
			socket.close();
		},
		/**
		 * set callback for socket error
		 * @param callback function to be called with error message on socket failure
		*/
		error : function(callback){
			error = callback;
		}
	}
}
