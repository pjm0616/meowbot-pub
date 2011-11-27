/*
	meowbot
	Copyright (C) 2008-2009 Park Jeong Min <pjm0616_at_gmail_d0t_com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <deque>
#include <sstream>

#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netdb.h> /* getprotobyname */
#include <netinet/in.h> //
#include <arpa/inet.h> //
#include <dlfcn.h>

#include <pcrecpp.h>
#include <boost/algorithm/string/replace.hpp>

#include "defs.h"
#include "module.h"
#include "tcpsock.h"
#include "etc.h"
#include "utf8.h"
#include "ircbot.h"


#define DEFAULT_USERAGENT "Mozilla/5.0 (X11; U; Linux i686; ko-KR; rv:1.9) Gecko/2008061015 Firefox/3.0"


// TODO
//http://news.google.co.kr/news?hl=ko&ned=kr&q=%s&output=atom
//http://newssearch.naver.com/search.naver?where=rss&query=%s // cp949



static std::string get_ipaddr_by_nick(ircbot_conn *isrv, const std::string &chan, const std::string &name)
{
	static pcrecpp::RE re_hidden_addr("[^.]+\\.users\\.HanIRC\\.org");
	
	if(unlikely(name.empty()))
		return "E;empty";
	
	isrv->rdlock_chanuserlist();
	const irc_strmap<std::string, irc_user> &users = isrv->get_chanuserlist(chan);
	irc_strmap<std::string, irc_user>::const_iterator it = users.find(name);
	
	std::string host(name);
	if(it != users.end())
	{
		host = it->second.gethost();
		if(re_hidden_addr.FullMatch(host))
		{
			isrv->unlock_chanuserlist();
			return "E;hidden";
		}
	}
	isrv->unlock_chanuserlist();
	
	return host;
	// return my_gethostbyname_s(host.c_str());
}

static std::string get_http_server_name(const std::string &server, int port)
{
	std::string buf, ua;
	tcpsock ts;
	
	if(port != 80 && port != 8080)
	{
		return "포트는 80, 8080만 허용됩니다.";
	}
	
	if(ts.connect(server.c_str(), port, 1000))
	{
		return "접속 실패";
	}
	ts << "HEAD / HTTP/1.1\r\n"
		"Host: " + server + "\r\n"
		"User-Agent: " DEFAULT_USERAGENT "\r\n"
		"Connection: close\r\n\r\n";
	
	while(ts.readline(buf, 1000) > 0) // break on socket error || timeout || empty line
	{
		if(pcrecpp::RE("Server: *(.*)").PartialMatch(buf, &ua))
		{
			break;
		}
	}
	
	return ua;
}

static int sock_read_all(tcpsock &sock, std::string &dest)
{
	char buf[1024];
	int ret;
	
	while((ret = sock.timed_read(buf, sizeof(buf), 3000)) > 0)
	{
		dest.append(buf, ret);
	}
	
	return 0;
}

static int get_url(const std::string &url, std::string &dest)
{
	url_parser urlp(url);
	tcpsock sock;
	int ret;
	
	ret = sock.connect(urlp.get_host(), urlp.get_port(), 4000);
	if(ret < 0)
		return -1;
	
	sock.printf("GET %s HTTP/1.1\r\n", urlp.get_location().c_str());
	sock.printf("Host: %s\r\n", urlp.get_host().c_str());
	sock.send("Accept-Encoding: *;q=0\r\n");
	sock.send("User-Agent: " DEFAULT_USERAGENT "\r\n");
	sock.send("Connection: close\r\n\r\n");
	while(sock.readline(dest, 4000) > 0){} // skip http header
	dest.clear();
	sock_read_all(sock, dest);
	sock.close();
	
	return 0;
}

static int google_search(const std::string &keyword, int start, std::vector<std::pair<std::string, std::string> > &dest)
{
	static pcrecpp::RE re_search_result("<nobr>[0-9]+\\. <a title=\"([^\"]*)\" href=([^>]+)>([^<]*)</a><br></nobr>");
	int ret;
	std::string data, content, url, title;
	
	ret = get_url("http://www.google.co.kr/ie?q=" + 
		urlencode(keyword) + "&hl=ko&start=" + int2string(start), data);
	if(ret < 0)
		return -1;
	
	pcrecpp::RE("</?em>").GlobalReplace("", &data);
	dest.clear();
	pcrecpp::StringPiece input(data);
	while(re_search_result.FindAndConsume(&input, &content, &url, &title))
	{
		title = decode_html_entities(title);
		title = decode_html_entities(title); // 두번 인코딩 돼있음
		content = decode_html_entities(content);
		content = decode_html_entities(content); // 두번 인코딩 돼있음
		dest.push_back(std::pair<std::string, std::string>(url, "[" + title + "] " + content));
	}
	
	return 0;
}

static std::string google_translate_detectlang(std::string msg)
{
	static pcrecpp::RE re_gt_metadata(", \"responseDetails\": (\"[^\"]*\"|null), \"responseStatus\": ([0-9]+)\\}"), 
		re_gt_lang("\\{\"responseData\": \\{\"language\":\"([^\"]+)\"");
	int ret, gt_status;
	std::string data, gt_detail, gt_result;
	
	if(msg.empty())
	{
		// return "";
		return "en";
	}
	msg = msg.substr(0, 350);
	ret = get_url("http://ajax.googleapis.com/ajax/services/language/detect?v=1.0&q=" + urlencode(msg), data);
	if(ret < 0)
	{
		// return "내부 오류";
		return "en";
	}
	
	//{"responseData": {"language":"ko","isReliable":true,"confidence":1.0}, "responseDetails": null, "responseStatus": 200}
	if(!re_gt_metadata.PartialMatch(data, &gt_detail, &gt_status))
	{
		// return "내부 오류";
		return "en";
	}
	if(gt_detail == "null") gt_detail = "";
	if(gt_status != 200)
	{
		// return "Language detection failed: " + gt_detail;
		return "en";
	}
	if(!re_gt_lang.PartialMatch(data, &gt_result) || !pcrecpp::RE("\"isReliable\":true").PartialMatch(data))
	{
		// return "Language detection failed";
		return "en";
	}
	
	return gt_result;
}


static bool gt_unescape_cb(std::string &dest, const std::vector<std::string> &groups, void *)
{
	dest += utf8_encode_s(my_atoi_base(groups[1].c_str(), 16));
	return true;
}

static std::string google_translate(std::string langfrom, std::string langto, std::string msg)
{
	static pcrecpp::RE re_validate_langpair("[a-z]{2}"), 
		re_gt_metadata(", \"responseDetails\": (\"[^\"]*\"|null), \"responseStatus\": ([0-9]+)\\}"), 
		re_gt_translated_text("\\{\"responseData\": \\{\"translatedText\":\"([^\"]+)\"\\}, ");
	int ret, gt_status;
	std::string data, gt_detail, gt_result;
	
	if(langfrom.empty())
	{
		langfrom = google_translate_detectlang(msg);
		if(!re_validate_langpair.FullMatch(langfrom))
		{
			return "오류: " + langfrom;
		}
	}
	if(langto.empty())
	{
		if(langfrom == "ko")
			langto = "en";
		else
			langto = "ko";
	}
	if(!re_validate_langpair.FullMatch(langfrom) || !re_validate_langpair.FullMatch(langto))
	{
		return "Invalid language";
	}
	msg = msg.substr(0, 350);
	ret = get_url("http://ajax.googleapis.com/ajax/services/language/translate?v=1.0&langpair=" + 
		langfrom + "%7C"/*|*/ +langto + "&q=" + urlencode(msg), data);
	if(ret < 0)
	{
		// return "내부 오류";
		return "en";
	}
	
	//{"responseData": {"translatedText":"야옹"}, "responseDetails": null, "responseStatus": 200}
	//{"responseData": null, "responseDetails": "invalid translation language pair", "responseStatus": 400}
	if(!re_gt_metadata.PartialMatch(data, &gt_detail, &gt_status))
	{
		return "내부 오류";
	}
	if(gt_detail == "null") gt_detail = "";
	if(gt_status != 200)
	{
		return "번역 실패: " + gt_detail;
	}
	if(!re_gt_translated_text.PartialMatch(data, &gt_result))
	{
		return "번역 실패";
	}
	pcrecpp::RE("\\\\u([0-9]{4})").GlobalReplaceCallback(&gt_result, gt_unescape_cb);
	
	return decode_html_entities(gt_result);
}

struct clubbox_filelist
{
	clubbox_filelist(){}
	clubbox_filelist(const std::string &name, const std::string &size, const std::string &url)
		: name(name), size(size), url(url)
	{}
	~clubbox_filelist(){}
	std::string name, size, url;
};

static int clubbox_search(std::vector<clubbox_filelist> &dest, const std::string &word, int max = -1)
{
	int ret, cnt;
	tcpsock ts, ts2;
	std::string buf, buf2;
	std::string filename, size, iurl, url;
	
	dest.clear();
	
	ret = ts.connect("filedic.com", 80, 1000);
	if(ret < 0) return -1;
	
	ts << "GET /index.html?title=" << urlencode(word) << " HTTP/1.1\r\n"
		"Host: filedic.com\r\n"
		"Accept-Encoding: *;q=0\r\n" 
		"User-Agent: " DEFAULT_USERAGENT "\r\n"
		"Connection: close\r\n\r\n";
	
	cnt = 0;
	while(ts.readline(buf, 500) >= 0)
	{
		pcrecpp::StringPiece input(buf);
		while(pcrecpp::RE("> ([^<>]+) \\(([^B]+B)\\) <a href=([^ ]+) target=_>\\[다운로드링크\\]</a> <a href='[^']+' target=_>\\[랭크박스링크\\]</a><br>")
			.FindAndConsume(&input, &filename, &size, &iurl))
		{
			ret = ts2.connect("filedic.com", 80, 1000);
			if(ret < 0) continue;
			
			ts2 << "GET /" << iurl << " HTTP/1.1\r\n"
				"Accept-Encoding: *;q=0\r\n" 
				"Host: filedic.com\r\n"
				"Connection: close\r\n\r\n";
			
			while(ts2.readline(buf2, 500) >= 0)
			{
				if(pcrecpp::RE("<META http-equiv='refresh' content='0; url=([^']+)'>")
					.PartialMatch(buf2, &url))
				{
					dest.push_back(clubbox_filelist(filename, size, url));
					break;
				}
			}
			
			ts2.close();
			
			cnt++;
			if(max > 0 && cnt >= max)
				goto end;
		}
	}
	
end:
	ts.close();
	
	return 0;
}


struct weather_data
{
	weather_data(){}
	weather_data(const std::string &fmtd_str)
		:fmtd_str(fmtd_str)
	{}
	~weather_data(){}
	
	std::string fmtd_str;
};

static int get_weather_data(std::map<std::string, weather_data> &dest)
{
	tcpsock ts;
	int ret;
	std::string buf;
	static pcrecpp::RE re_wdata_parser(" *<local stn_id=\"[^\"]*\" stn_ko=\"([^\"]*)\" stn_en=\"[^\"]*\""
		" ww=\"[^\"]*\" ww_ko=\"([^\"]*)\" vs=\"([^\"]*)\" ca_tot=\"([^\"]*)\""
		" ta=\"([^\"]*)\" wd=\"([^\"]*)\" ws=\"([^\"]*)\" hm=\"([^\"]*)\" rn_day=\"([^\"]*)\""
		" ps=\"([^\"]*)\" wh=\"([^\"]*)\">[^<]+</local>");
	
	dest.clear();
	ret = ts.connect("kma.go.kr", 80, 2000);
	if(ret < 0)
	{
		return -1;
	}
	ts << "GET /weather/xml/sfc_web_now.xml HTTP/1.1\r\n"
		"Host: www.kma.go.kr\r\n"
		"Accept-Encoding: *;q=0\r\n" 
		"User-Agent: " DEFAULT_USERAGENT "\r\n"
		"Connection: close\r\n\r\n";
	
	while(ts.readline(buf, 500) > 0){} ts.readline(buf, 500); // skip http header
	while(ts.readline(buf, 500) >= 0)
	{
		std::string name, weather_desc, vs, ca_tot, temperature, wind, wind_velocity, humidity, rain_day, pressure, wh;
		// <local stn_id="156" stn_ko="광주" stn_en="Gwangju" ww="rain" ww_ko="약한비단속" vs="11" ca_tot="10" ta="23.4" wd="NNE" ws="1.4" hm="81" rn_day="0.0" ps="1015.4" wh="">Gwangju</local>
		// <local stn_id="165" stn_ko="목포" stn_en="Mokpo" ww="preceding rain" ww_ko="비 끝남" vs="8" ca_tot="10" ta="22.8" wd="NE" ws="1.2" hm="96" rn_day="0.0" ps="1015.1" wh="0.5">Mokpo</local>
		if(re_wdata_parser.FullMatch(buf, 
			&name, &weather_desc, &vs, &ca_tot, &temperature, &wind, 
			&wind_velocity, &humidity, &rain_day, &pressure, &wh))
		{
			std::string wind_ko = wind;
			boost::algorithm::replace_all(wind_ko, "N", "북");
			boost::algorithm::replace_all(wind_ko, "E", "동");
			boost::algorithm::replace_all(wind_ko, "W", "서");
			boost::algorithm::replace_all(wind_ko, "S", "남");
			
			if(weather_desc == "박무") weather_desc = "안개";
			else if(weather_desc == "연무") weather_desc = "흐린 안개";
			if(wind_velocity.empty()) wind_velocity = "0";
			if(humidity.empty()) humidity = "0";
			if(rain_day.empty()) rain_day = "0";
			
			dest[name] = weather_data(
				"온도: " + temperature + "℃, "
				"풍향: " + wind_ko + ", "
				"풍속: " + wind_velocity + "m/s, " /* "㎧, " */
				"습도: " + humidity + "%, "
				"일 강수량: " + rain_day + "mm"
				"; " + weather_desc
				);
		}
	}
	ts.close();
	
	return 0;
}

static std::string get_whois_data_plain_internal(const std::string &whoisservername, const std::string &host)
{
	tcpsock ts;
	std::string buf, result;
	const char *whoisserver, *encoding = NULL;
	
	if(whoisservername == "ARIN")
		whoisserver = "whois.arin.net";
	else if(whoisservername == "RIPE")
		whoisserver = "whois.ripe.net";
	else if(whoisservername == "APNIC")
		whoisserver = "whois.apnic.net";
	else if(whoisservername == "KRNIC")
	{
		whoisserver = "whois.nic.or.kr";
		encoding = "cp949";
	}
	else if(whoisservername == "JPNIC")
	{
		whoisserver = "whois.nic.ad.jp";
		encoding = "shift-jis";
	}
	else if(whoisservername == "RADB")
	{
		whoisserver = "whois.radb.net";
	}
	else
		return "";
	
	if(encoding)
		ts.set_encoding(encoding);
	ts.connect(whoisserver, 43, 8000);
	ts << host << "\r\n";
	while(ts.readline(buf, 8000) >= 0)
	{
		result += buf + "\n";
	}
	ts.close();
	
	return result;
}

// whois result is stored in data, returns whois server name
static std::string get_whois_data_plain(const std::string &host, std::string &data, 
	const char *default_whoissrv_name = "ARIN")
{
	std::string whoissrvname = default_whoissrv_name;
	
LABEL_getdata:
	data = get_whois_data_plain_internal(whoissrvname, host);
	if(data.empty())
	{
		return "";
	}
	if(whoissrvname == "ARIN" && (
		strstr(data.c_str(), "be found in the RIPE database at whois.ripe.net") || 
		strstr(data.c_str(), "European Regional Internet Registry/RIPE NCC") || 
		strstr(data.c_str(), "RIPE Network Coordination Centre")))
	{
		whoissrvname = "RIPE";
		goto LABEL_getdata;
	}
	else if(whoissrvname == "ARIN" && (
		strstr(data.c_str(), "refer to the APNIC Whois Database")))
	{
		whoissrvname = "APNIC";
		goto LABEL_getdata;
	}
	else if(whoissrvname == "APNIC" && (
		strstr(data.c_str(), "For more information, using KRNIC Whois Database") || 
		strstr(data.c_str(), "please refer to the KRNIC Whois DB") || 
		strstr(data.c_str(), "please refer to the NIDA Whois DB") || 
		strstr(data.c_str(), "the KRNIC Whois Database at:")))
	{
		whoissrvname = "KRNIC";
		goto LABEL_getdata;
	}
	else if(whoissrvname == "APNIC" && (
		strstr(data.c_str(), "JPNIC whois server at whois.nic.ad.jp") || 
		strstr(data.c_str(), "Japan Network Information Center (NETBLK-JAPAN-NET)")))
	{
		whoissrvname = "JPNIC";
		goto LABEL_getdata;
	}
#if 0
	else if(whoissrvname == "KRNIC" && (
		strstr(data.c_str(), "조회하신 AS번호는 국내에 할당된 AS 번호가 아닙니다.")))
	{
		
	}
#endif
	
	return whoissrvname;
}

static int parse_whois_data(const std::string &whoissrvname, const std::string &data, 
	std::map<std::string, std::string> &dest)
{
	static pcrecpp::RE_Options re_opts(PCRE_MULTILINE);
	
	dest["source"] = whoissrvname;
	dest["error"] = "";
	if(whoissrvname == "ARIN")
	{
		if(strstr(data.c_str(), "No match found for "))
		{
			return -1;
		}
		pcrecpp::RE("OrgName:\\s+(.+)", re_opts).PartialMatch(data, &dest["owner"]);
		pcrecpp::RE("NetName:\\s+(.+)", re_opts).PartialMatch(data, &dest["netname"]);
		if(dest["owner"].empty() || dest["netname"].empty())
			pcrecpp::RE("(.+) (.+) \\(NET-", re_opts).PartialMatch(data, &dest["owner"], &dest["netname"]);
		
		if(!pcrecpp::RE("NetRange:\\s+([0-9.]+) - ([0-9.]+)\n")
			.PartialMatch(data, &dest["netrange_start"], &dest["netrange_end"]))
		{
			pcrecpp::RE("([0-9.]+) - ([0-9.]+)").PartialMatch(data, &dest["netrange_start"], &dest["netrange_end"]);
		}
	}
	else if(whoissrvname == "RIPE" || whoissrvname == "APNIC")
	{
		pcrecpp::RE("inetnum:\\s+([0-9.]+) {1,2}- {1,2}([0-9.]+)\n")
			.PartialMatch(data, &dest["netrange_start"], &dest["netrange_end"]);
		pcrecpp::RE("netname:\\s+(.+)", re_opts).PartialMatch(data, &dest["netname"]);
		pcrecpp::RE("descr:\\s+(.+)", re_opts).PartialMatch(data, &dest["owner"]);
		pcrecpp::RE("route:[ \t0-9./]+\ndescr:\\s+(.*)\n").PartialMatch(data, &dest["route"]);
	}
	else if(whoissrvname == "KRNIC")
	{
		if(!pcrecpp::RE("IPv4주소\\s+: ([0-9.]+)-([0-9.]+)\n")
			.PartialMatch(data, &dest["netrange_start"], &dest["netrange_end"]))
		{
			pcrecpp::RE("서비스명\\s+: (.+)", re_opts).PartialMatch(data, &dest["netname"]);
			pcrecpp::RE("기 관 명\\s+: (.+)", re_opts).PartialMatch(data, &dest["owner"]);
		}
		else
		{
			pcrecpp::RE("네트워크 이름\\s+: (.+)", re_opts).PartialMatch(data, &dest["netname"]);
			pcrecpp::RE("기관명\\s+: (.+)", re_opts).PartialMatch(data, &dest["owner"]);
			pcrecpp::RE("연결 ISP명\\s+: (.+)\n").PartialMatch(data, &dest["route"]);
		}
#if 1 /////// AS조회
		pcrecpp::RE("^AS 이름 +: (.+)", re_opts).PartialMatch(data, &dest["AS이름"]);
		pcrecpp::RE("^기관명 +: (.+)", re_opts).PartialMatch(data, &dest["기관명"]);
		
		if(strstr(data.c_str(), "조회하신 AS번호는 국내에 할당된 AS 번호가 아닙니다.")) // FIXME
		{
			if(strstr(data.c_str(), "IANA"))
				dest["error"] = "IANA에 할당된 AS번호입니다.";
			else if(strstr(data.c_str(), "ARIN"))
				dest["error"] = "ARIN에 할당된 AS번호입니다.";
			else if(strstr(data.c_str(), "RIPE"))
				dest["error"] = "RIPE에 할당된 AS번호입니다.";
			else if(strstr(data.c_str(), "APNIC"))
				dest["error"] = "APNIC에 할당된 AS번호입니다.";
			else
				dest["error"] = "기타 다른 곳에 할당된 AS번호입니다.";
		}
		else if(strstr(data.c_str(), "질의 내용이 부정확 합니다."))
		{
			dest["error"] = "찾을 수 없습니다.";
		}
		else if(strstr(data.c_str(), "본 조회는 조회 형식이 옳지 않습니다."))
		{
			dest["error"] = "올바르지 않은 형식입니다.";
		}
#endif
	}
	else if(whoissrvname == "JPNIC")
	{
		if(!pcrecpp::RE("a\\. \\[Network Number\\]\\s+([0-9.]+)-([0-9.]+)\n")
			.PartialMatch(data, &dest["netrange_start"], &dest["netrange_end"]))
		{
			pcrecpp::RE("a\\. \\[Network Number\\]\\s+([0-9.]+)\n").PartialMatch(data, &dest["netrange_start"]);
		}
		pcrecpp::RE("b\\. \\[Network Name\\]\\s+(.+)", re_opts).PartialMatch(data, &dest["netname"]);
		pcrecpp::RE("g\\. \\[Organization\\]\\s+(.+)", re_opts).PartialMatch(data, &dest["owner"]);
	}
	
	return 0;
}

static int get_whois_data(const std::string &host, std::map<std::string, std::string> &result, 
	const char *default_whoissrv_name = "ARIN")
{
	std::string data;
	result.clear();
	result["error"] = "";
	const std::string &whoissrvname = get_whois_data_plain(host, data, default_whoissrv_name);
	if(whoissrvname.empty())
	{
		return -2;
	}
	
	int ret = parse_whois_data(whoissrvname, data, result);
	result["host"] = host;
	if(!result["error"].empty())
		return -3;
	
	return ret;
}


#if 0
// IP 차단됨-_-
static std::map<std::string, float> get_currency()
{
	static pcrecpp::RE	re_start("[ \t]*<td class=\"fx_tableATitle04\"><a [^>]+><!--[^>]+>([^<]+)</a></td>\r?"), 
						re_currency("[ \t]*<td class=\"fx_tableAText04R\">([.0-9]+)</td>\r?"), 
						re_strip_dummy_chars("　" /*\xe3\x80\x80*/);
	int ret;
	std::string data, line;
	std::map<std::string, float> result;
	
	ret = get_url("http://ebank.keb.co.kr/IBS/servlet/com.etc.FXSvl2?"
		"sOutPage=/b2c/cspecial/csexchange/csex001r.jsp&cmd=PE060&"
		"sGubun=1&sGubun1=1&sVisable=true&YSTDATE=", data);
	if(ret < 0)
	{
		return result;
	}
	data = encconv::convert("cp949", "utf-8", data);
	
	std::stringstream ss(data);
	int mode = 0, idx;
	std::string name;
	float currency;
	
	while(ss.good())
	{
		std::getline(ss, line);
		if(mode == 0)
		{
			if(re_start.FullMatch(line, &name))
			{
				re_strip_dummy_chars.GlobalReplace("", &name);
				mode = 1;
				idx = 0;
			}
		}
		else if(mode == 1)
		{
			if(re_currency.FullMatch(line, &currency))
			{
				if(idx == 6)
				{
					result[name] = currency;
					mode = 0;
				}
				else
				{
					idx++;
				}
			}
		}
	}
	if(mode != 0)
	{
		result.clear();
		return result;
	}
	
	return result;
}
#else
static std::map<std::string, float> get_currency()
{
	static pcrecpp::RE	re_start("[ \t]*<td [^>]+><a href='/index/exchange_detail\\.nhn\\?code=[0-9]+'>([^<]+)</a></td>\r?"), 
						re_currency("^[ \t]*<td class='tb04_num02'>([,.0-9]+)</td>\r?"), 
						re_strip_comma(",");
	int ret;
	std::string data, line;
	std::map<std::string, float> result;
	
	ret = get_url("http://bank.naver.com/index/exchange.nhn", data);
	if(ret < 0)
	{
		return result;
	}
	data = encconv::convert("cp949", "utf-8", data);
	
	std::stringstream ss(data);
	int mode = 0, idx;
	std::string name, buf;
	
	while(ss.good())
	{
		std::getline(ss, line);
		if(mode == 0)
		{
			if(re_start.Match(line, &name))
			{
				mode = 1;
				idx = 0;
			}
		}
		else if(mode == 1)
		{
			if(re_currency.Match(line, &buf))
			{
				if(idx == 5)
				{
					re_strip_comma.GlobalReplace("", &buf);
					result[name] = atof(buf.c_str());
					mode = 0;
				}
				else
				{
					idx++;
				}
			}
		}
	}
	if(mode != 0)
	{
		result.clear();
		return result;
	}
	
	return result;
}
#endif


struct tv_schedule_data
{
	tv_schedule_data(){}
	tv_schedule_data(const std::string &id, const std::string &name, 
			const std::string &channel, const std::string &starttime_str, time_t starttime, 
			const std::string &endtime_str, time_t endtime, const std::string &type)
		:id(id), name(name), channel(channel), starttime_str(starttime_str), endtime_str(endtime_str), 
		type(type), starttime(starttime), endtime(endtime)
	{}
	~tv_schedule_data(){}
	
	std::string id, name, channel, starttime_str, endtime_str, type;
	time_t starttime, endtime;
};
static bool tv_sched_sort_comparator(const tv_schedule_data &d1, const tv_schedule_data &d2)
{
	return d1.starttime < d2.starttime;
}

static std::vector<tv_schedule_data> get_tv_schedule()
{
	static pcrecpp::RE	re_schedule("<a href=\"JavaScript:ViewContent\\('([^']+)'\\)\" onMouseOver=\"Preview\\('[^']*','([^']*)','([^']*)','([^'<]*)<br>~([^']*)','([^']*)','[^']*','[^']*'\\)\" onMouseOut=\"[^\"]*\">", pcrecpp::RE_Options(PCRE_UTF8 | PCRE_MULTILINE)), 
							// id, name, chan, start, end, type
						re_date("([0-9]+)/([0-9]+) (AM|PM) ([0-9]+):([0-9]+)");
							// 11/22 PM 1:30
	int ret;
	std::string data;
	std::vector<tv_schedule_data> result;
	std::string id, name, channel, starttime_str, endtime_str, type;
	time_t starttime, endtime;
	int month, day, hour, minute;
	std::string ampm;
	time_t now;
	struct tm tm_now, tm_tmp;
	
	ret = get_url("http://www.epg.co.kr/new/tvguide/tvguide.php", data);
	if(ret < 0)
	{
		return result;
	}
	data = encconv::convert("cp949", "utf-8", data);
	
	// 현재의 年을 구함
	{
		now = time(NULL);
		localtime_r(&now, &tm_now);
		tm_tmp.tm_sec = 0;
		tm_tmp.tm_year = tm_now.tm_year;
		tm_tmp.tm_isdst = 0;
	}
	
	pcrecpp::StringPiece input(data);
	while(re_schedule.FindAndConsume(&input, &id, &name, &channel, &starttime_str, &endtime_str, &type))
	{
		name = decode_html_entities(name);
		
		if(!re_date.FullMatch(starttime_str, &month, &day, &ampm, &hour, &minute))
			continue;
		if(ampm == "PM")
			hour += 12;
		tm_tmp.tm_min = minute; tm_tmp.tm_hour = hour; tm_tmp.tm_mday = day; tm_tmp.tm_mon = month - 1;
		starttime = mktime(&tm_tmp);
		
		if(!re_date.FullMatch(endtime_str, &month, &day, &ampm, &hour, &minute))
			continue;
		if(ampm == "PM")
			hour += 12;
		tm_tmp.tm_min = minute; tm_tmp.tm_hour = hour; tm_tmp.tm_mday = day; tm_tmp.tm_mon = month - 1;
		endtime = mktime(&tm_tmp);
		
		result.push_back(tv_schedule_data(id, name, channel, starttime_str, starttime, endtime_str, endtime, type));
	}
	// starttime을 기준으로 정렬
	std::sort(result.begin(), result.end(), tv_sched_sort_comparator);
	
	return result;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////







static int cmd_cb_google(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	if(carg.empty())
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <검색할 단어> | !ng: 검색 결과를 노티스로 보냄");
		return 0;
	}
	
	std::vector<std::pair<std::string, std::string> > srchres;
	if(!google_search(carg, 0, srchres))
	{
		if(srchres.begin() == srchres.end())
		{
			isrv->privmsg(mdest, "검색 결과가 없습니다.");
		}
		else
		{
			std::vector<std::pair<std::string, std::string> >::const_iterator it;
			int i;
			if(!strcasecmp(cmd.c_str(), "ng"))
			{
				isrv->privmsg_nh(mdest, "http://www.google.co.kr/search?hl=ko&q=" + urlencode(carg));
				for(i = 0, it = srchres.begin(); it != srchres.end() && i < isrv->bot->max_lines_per_cmd; i++, it++)
				{
					// notice로 보냄
					isrv->privmsg(irc_msginfo(pmsg.getnick(), IRC_MSGTYPE_NOTICE), 
						std::string(it->first + " - " + it->second).substr(0, 300));
				}
			}
			else
			{
				for(i = 0, it = srchres.begin(); it != srchres.end() && i < isrv->bot->max_lines_per_cmd; i++, it++)
				{
					isrv->privmsg(mdest, std::string(it->first + " - " + it->second).substr(0, 300));
				}
			}
		}
	}
	else
	{
		isrv->privmsg(mdest, "내부 오류");
	}
	
	return 0;
}



static int cmd_cb_websrv_name(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	std::string tmp, tmp2;
	
	if(!pcrecpp::RE("([^ ]+) ?([0-9]+)?").FullMatch(carg, &tmp, &tmp2))
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <서버> [포트]");
		return HANDLER_FINISHED;
	}
	int port = atoi(tmp2.c_str());
	if(!port) port = 80;
	isrv->privmsg_nh(mdest, get_http_server_name(tmp, port).substr(0, 150));
	
	return 0;
}

static int cmd_cb_translate(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	static pcrecpp::RE re_extract_langpair("(ko|en|ja|zh|fr|de|es|it|ru|pt|한|영|일|중|불|독|에|이|러|포)((?1)) (.+)");
	std::string langpair[2], str;
	
	if(carg.empty())
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <번역할 글>");
		isrv->privmsg(mdest, "사용법: !" + cmd + " (<원문 언어><번역할 언어>) <번역할 글>");
		isrv->privmsg(mdest, "예: !번역 한영 야옹");
		return 0;
	}
	if(!re_extract_langpair.FullMatch(carg, &langpair[0], &langpair[1], &str))
		str = carg;
	
	for(int i = 0; i < 2; i++)
	{
		if(langpair[i] == "한") langpair[i] = "ko";
		else if(langpair[i] == "영") langpair[i] = "en";
		else if(langpair[i] == "일") langpair[i] = "ja";
		else if(langpair[i] == "중") langpair[i] = "zh";
		else if(langpair[i] == "불") langpair[i] = "fr";
		else if(langpair[i] == "독") langpair[i] = "de";
		else if(langpair[i] == "에") langpair[i] = "es";
		else if(langpair[i] == "이") langpair[i] = "it";
		else if(langpair[i] == "러") langpair[i] = "ru";
		else if(langpair[i] == "포") langpair[i] = "pt";
	}
	isrv->privmsg_nh(mdest, " " + google_translate(langpair[0], langpair[1], str));
	
	return 0;
}

static int cmd_cb_clubbox_search(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	int ret;
	
	if(carg.empty())
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <검색어>");
		return 0;
	}
	
	std::vector<clubbox_filelist> list;
	ret = clubbox_search(list, carg, isrv->bot->max_lines_per_cmd);
	if(ret < 0)
		isrv->privmsg(mdest, "내부 오류");
	else if(list.begin() == list.end())
		isrv->privmsg(mdest, "검색 결과가 없습니다.");
	else
	{
		int i;
		std::vector<clubbox_filelist>::const_iterator it;
		for(it = list.begin(), i = 0; 
			it != list.end() && i < isrv->bot->max_lines_per_cmd; 
			it++, i++)
		{
			isrv->privmsgf(mdest, "%s - %.150s (%.20s)", 
				it->url.c_str(), it->name.c_str(), it->size.c_str());
		}
	}
	
	return 0;
}


static int cmd_cb_weather(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	int ret;
	(void)cmd;
	
	if(carg.empty())
	{
		isrv->privmsg(mdest, "사용법: !날씨 <지역>, !날씨 목록");
		return 0;
	}
	
	std::map<std::string, weather_data> data;
	ret = get_weather_data(data);
	if(ret < 0)
		isrv->privmsg(mdest, "접속 실패");
	else if(data.begin() == data.end())
		isrv->privmsg(mdest, "내부 오류");
	else
	{
		std::map<std::string, weather_data>::const_iterator it;
		if(carg == "목록")
		{
			std::string buf;
			int i;
			for(i = 1, it = data.begin(); it != data.end(); it++, i++)
			{
				buf += it->first + ", ";
				if(i % 40 == 0)
				{
					isrv->privmsg(mdest, buf);
					buf.clear();
				}
			}
			if(!buf.empty())
				isrv->privmsg(mdest, buf);
		}
		else
		{
			it = data.find(carg);
			if(it == data.end())
				isrv->privmsg(mdest, "해당 지역에 대한 정보가 없습니다.");
			else
				isrv->privmsg_nh(mdest, it->second.fmtd_str);
		}
	}
	
	return 0;
}

static int cmd_cb_ipw(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	static pcrecpp::RE re_ipv4("(\\d+\\.\\d+\\.\\d+\\.\\d+)");
	const std::string &chan = pmsg.getchan();
	std::string host;
	
	if(carg.empty())
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <IP/닉네임>");
		return 0;
	}
	if(!re_ipv4.PartialMatch(carg, &host))
	{
		isrv->rdlock_chanuserlist();
		const irc_strmap<std::string, irc_user> &users = isrv->get_chanuserlist(chan);
		std::string nick(carg);
		boost::algorithm::replace_all(nick, " ", "");
		irc_strmap<std::string, irc_user>::const_iterator it = users.find(nick);
		if(it == users.end())
		{
			pcrecpp::RE("([^ \t]+)").PartialMatch(carg, &host);
		}
		else
		{
			host = it->second.gethost();
			if(pcrecpp::RE("[^.]+\\.users\\.HanIRC\\.org").FullMatch(host))
			{
				isrv->privmsg(mdest, "이 사용자는 IP 주소를 숨기고 있습니다.");
				isrv->unlock_chanuserlist();
				return 0;
			}
		}
		isrv->unlock_chanuserlist();
	}
	
	host = my_gethostbyname_s(host.c_str());
	if(host.empty())
	{
		isrv->privmsgf_nh(mdest, " %s: IP를 찾을 수 없습니다.", carg.c_str());
		return 0;
	}
	
	std::map<std::string, std::string> result;
	int ret = get_whois_data(host, result);
	if(ret == -2)
	{
		isrv->privmsg(mdest, "내부 오류");
		return 0;
	}
	else if(ret < 0) // ret == -1
	{
		isrv->privmsg(mdest, " " + host + "에 대한 정보를 찾을 수 없습니다.");
		return 0;
	}
	
	isrv->privmsgf_nh(mdest, "[%s] %s : %s (%s%s%s, %s-%s)", 
		result["source"].c_str(), result["host"].c_str(), result["owner"].c_str(), 
		result["route"].c_str(), result["route"].empty()?"":"/", result["netname"].c_str(), 
		result["netrange_start"].c_str(), result["netrange_end"].c_str()
		);
	
	return 0;
}

static int cmd_cb_asw(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	std::string qstr(carg);
	
	if(carg.empty())
	{
		isrv->privmsg(mdest, "AS번호를 조회합니다. 현재는 KRNIC만 사용 가능합니다. 사용법: !" + cmd + " <AS 번호>");
		return 0;
	}
	
	if(irc_strlower(qstr.substr(0, 2)) != "as") // irc_tolower이 아니라 std::string 용 일반 tolower를 써야하지만, 귀찮아서 [..]
		qstr = "as"+qstr;
	std::map<std::string, std::string> result;
	int ret = get_whois_data(qstr, result, "KRNIC");
	if(ret == -2)
	{
		isrv->privmsg(mdest, "내부 오류");
		return 0;
	}
	else if(ret == -3)
	{
		isrv->privmsg(mdest, qstr + ": " + result["error"]);
		return 0;
	}
	else if(ret < 0) // ret == -1
	{
		isrv->privmsg(mdest, qstr + "에 대한 정보를 찾을 수 없습니다.");
		return 0;
	}
	
	isrv->privmsgf_nh(mdest, "[" IRCTEXT_UNDERLINE "%s" IRCTEXT_NORMAL "] %s : %s / " IRCTEXT_BOLD "%s" IRCTEXT_NORMAL, 
		result["source"].c_str(), qstr.c_str(), result["AS이름"].c_str(), result["기관명"].c_str()
		);
	
	return 0;
}

static int cmd_cb_exchange(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)cmd;
	std::map<std::string, float> crncy;
	std::map<std::string, float>::const_iterator it;
	
	if(carg.empty())
	{
		isrv->privmsg_nh(mdest, "사용법: !환율 <이름(USD, 등)>");
		isrv->privmsg_nh(mdest, "사용법: !환율 목록");
		
		return 0;
	}
	if(carg == "목록")
	{
		crncy = get_currency();
		std::string buf;
		int i = 0;
		for(it = crncy.begin(); it != crncy.end(); it++)
		{
			buf += it->first + ", ";
			if((i + 1) % 10 == 0)
			{
				isrv->privmsg(mdest, buf);
				buf.clear();
			}
			i++;
		}
		if(!buf.empty())
			isrv->privmsg(mdest, buf);
		
		return 0;
	}
	
	crncy = get_currency();
	for(it = crncy.begin(); it != crncy.end(); it++)
	{
		if(strcasestr(it->first.c_str(), carg.c_str()))
		{
			break;
		}
	}
	if(it == crncy.end())
	{
		isrv->privmsgf_nh(mdest, "오류: %s에 대한 정보를 찾을 수 없습니다.", carg.c_str());
		return 0;
	}
	isrv->privmsgf_nh(mdest, "%s: %.02f원", it->first.c_str(), it->second);
	
	return 0;
}

static int cmd_cb_tv_schedule(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)cmd;
	time_t ctime = time(NULL);
	
	if(carg.empty())
	{
		isrv->privmsg(mdest, "사용법: !TV편성표 <이름>");
		return 0;
	}
	
	std::vector<tv_schedule_data> sdata = get_tv_schedule();
	std::vector<tv_schedule_data>::const_iterator it;
	// id, name, channel, starttime_str, endtime_str, type
	for(it = sdata.begin(); it != sdata.end(); it++)
	{
		if(strcasestr(strip_spaces(it->name).c_str(), strip_spaces(carg).c_str()) && (it->starttime >= ctime))
		{
			isrv->privmsgf(mdest, "%s - %s (%s ~ %s)", it->name.c_str(), it->channel.c_str(), 
				it->starttime_str.c_str(), it->endtime_str.c_str());
			return 0;
		}
	}
	isrv->privmsgf(mdest, " %s에 대한 검색결과가 없습니다.", carg.c_str());
	
	return 0;
}




int mod_netutils_init(ircbot *bot)
{
	bot->register_cmd("구글", cmd_cb_google);
	bot->register_cmd("google", cmd_cb_google);
	bot->register_cmd("gg", cmd_cb_google);
	bot->register_cmd("ng", cmd_cb_google);
	bot->register_cmd("웹서버", cmd_cb_websrv_name);
	bot->register_cmd("번역", cmd_cb_translate);
	bot->register_cmd("tr", cmd_cb_translate);
	bot->register_cmd("클박", cmd_cb_clubbox_search);
	bot->register_cmd("날씨", cmd_cb_weather);
	bot->register_cmd("ipw", cmd_cb_ipw);
	bot->register_cmd("asw", cmd_cb_asw);
	bot->register_cmd("환율", cmd_cb_exchange);
	bot->register_cmd("tv편성표", cmd_cb_tv_schedule);
	
	return 0;
}

int mod_netutils_cleanup(ircbot *bot)
{
	bot->unregister_cmd("구글");
	bot->unregister_cmd("google");
	bot->unregister_cmd("gg");
	bot->unregister_cmd("ng");
	bot->unregister_cmd("웹서버");
	bot->unregister_cmd("번역");
	bot->unregister_cmd("tr");
	bot->unregister_cmd("클박");
	bot->unregister_cmd("날씨");
	bot->unregister_cmd("ipw");
	bot->unregister_cmd("asw");
	bot->unregister_cmd("환율");
	bot->unregister_cmd("tv편성표");
	
	return 0;
}



MODULE_INFO(mod_netutils, mod_netutils_init, mod_netutils_cleanup)




