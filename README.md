# mod_shared_hosting

A reimplementation of Apache's mod_vhost_alias. Additional configuration options have been added to make shared hosting on a single server easier.

This module allows for using multiple paths to be tested against the HTTP hostname, which allows for things like sub directories to become sub domains, or a default "under construction" directory to be specified if the document root directory can not be found.

This module also supports suexec. If its enabled it will set the UID/GID to the document root owner UID/GID.

This module does not use IP address configuration options available in mod_vhost_alias.

## Status

This code hasn't been updated or used in production for some time, but did run on load balanced web servers that hosted over 80k websites without any problems.

## Installation

When compiling apache:

	$ ./configure \
		..
		--enable-module=../mod_shared_hosting/mod_shared_hosting.c
	$ make install

Using apxs:

 	$ sudo apxs -i -a -c mod_shared_hosting.c 
 	...
 	[activating module `shared_hosting' in /opt/local/apache2/conf/httpd.conf]

## Usage

Configure apache's httpd.conf:

	<VirtualHost *:80>
		VirtualDocumentRoots /www/vhosts/%0.1/%0/htdocs /www/vhosts/%0.1/%0/%1/htdocs
		VirtualScriptAliases /cgi-bin /www/vhosts/%0.1/%0/cgi-bin /www/vhosts/%0.1/%0/%1/cgi-bin /www/cgi-bin
		Suexec On
	</VirtualHost>


## Credits

Author: James L Moser

Code originally based on apache's own mod_vhost_alias.

## License

The MIT License (MIT)

Copyright (c) 2015 James L Moser

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.