# How to Install


### 1) Compile the web socket server

This source can be compiled using make, which most of you should already be familiar with. There is no configure script (yet), so installing is as easy as `make socket`. There is currently no `make install`, but that will hopefully change in the future.



### 2) Set up your web server (NGINX recommended)

Now, before we run the web socket server, we have to set up our web server. I highly recommend NGINX for this because using Apache to proxy sockets has been very problematic for me personally.

Here is an example of how your NGINX config should look, with the "location" block being the magic part required to make the web sockets work:

	server {
		listen 80;

		root /var/www/chattor;
		server_name chattorci7bcgygp.onion;

		location /ws/chat/{
			proxy_read_timeout 31536000;
			proxy_connect_timeout 31536000;

			proxy_pass http://127.0.0.1:8080;
		}

		access_log /dev/null refs;
	}

Obviously, replace the .onion with your clearnet or onion domain, replace the port numbers as necessary, and (hopefully) set your log file to /dev/null so no logs are kept.

**IMPORTANT**: Your web server port and the web socket server port need to be different. In this case, NGINX is port 80 and the web socket server is port 8080.



### 3) Start the web socket server

Finally, to run your newly-compiled web socket server, type `./bin/web_socket_server 8080`. If you'd like to run the process via SSH or a local terminal and be able to exit the session without killing the process, instead use `nohup ./bin/web_socket_server 8080 &`. There are currently no service files for systemd nor any other init system, but, again this will likely change as the install process improves.



# Conclusion

Thanks for showing an interest in my source. I'm new to the open-source community, so please be patient with me and, if you have the time, help me to improve my code and my packaging in general.


Thanks,
  ChatTor
