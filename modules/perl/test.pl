#!/usr/bin/perl

#	meowbot
#	Copyright (C) 2008-2009 Park Jeong Min <pjm0616_at_gmail_d0t_com>
#
#	This program is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.


sub cmd_cb
{
	my ($chan, $ident, $cmd, $carg) = @_;
	meowbot_privmsg_nh($chan, "$chan, $ident, $cmd, $carg");
}


#print "load\n";

sub init
{
	#print "init\n";
	meowbot_register_cmd('pt', \&cmd_cb);
}


sub cleanup
{
	#print "cleanup\n";
	meowbot_unregister_cmd('pt');
}




