#!/usr/bin/php
<?php

/*  File name : ChoseongTrifler.php
 *  Purpose   : Moves Choseong to previous syllable.
 *  Architect : Cyrus Hwang (cyrushwang@macbookers.net)
 *  Created   : 2008-09-02
 *  Last edit : 2008-09-11
 *  Copyright : Cyrus Hwang (http://uncyclopedia.kr/wiki/User:CyrusHwang)
 *  License   : General Public License
 */

class ChoseongTrifler {
	
	private function CPUTF8($cpInt) { // Converts provided Unicode codepoint(int) into UTF-8 character(char).
		if($cpInt>=0 && $cpInt<=127) {
			$rtnutf8=chr($cpInt);
		} elseif($cpInt>=128 && $cpInt<=2047) {
			$rtnutf8=chr(($t=$cpInt>>6)+192).chr(($cpInt-($t<<6))+128);
		} elseif($cpInt>=2048 && $cpInt<=65535) {
			$rtnutf8=chr(($t=$cpInt>>12)+224).chr(($p=($m=($cpInt-($t<<12)))>>6)+128).chr(($m-($p<<6))+128);
		} elseif($cpInt>=65536 && $cpInt<=1114111) {
			$rtnutf8=chr(($t=$cpInt>>18)+240).chr(($p=($m=($cpInt-($t<<18)))>>12)+128).chr(($a=($v=($m-($p<<12)))>>6)+128).chr(($v-($a<<6))+128);
		}
		
		return $rtnutf8;
	}
	
	private function UTF8CP($utf8char) { // Converts provided UTF-8 character(char) into Unicode codepoint(int).
		$rng=strlen($utf8char);
		switch($rng) {
			case 1:
				$rtncp=ord($utf8char);
				break;
			case 2:
				$fc=ord(substr($utf8char,0,1));
				$ec=ord(substr($utf8char,1,1));
				$rtncp=(($fc-192)<<6)+($ec-128);
				break;
			case 3:
				$fc=ord(substr($utf8char,0,1));
				$mc=ord(substr($utf8char,1,1));
				$ec=ord(substr($utf8char,2,1));
				$rtncp=(($fc-224)<<12)+(($mc-128)<<6)+($ec-128);
				break;
			case 4:
				$fc=ord(substr($utf8char,0,1));
				$lc=ord(substr($utf8char,1,1));
				$hc=ord(substr($utf8char,2,1));
				$ec=ord(substr($utf8char,3,1));
				$rtncp=(($fc-240)<<18)+(($lc-128)<<12)+(($hc-128)<<6)+($ec-128);
				break;
		}
		
		return $rtncp;
	}
	
	private function Disorganize($cp) {
		$cp-=44032;
		$tmp_e=$cp%28;
		$tmp_m=(($cp-$tmp_e)/28)%21;
		$tmp_s=($cp-(($tmp_m*28)+$tmp_e))/588;
		
		return array($tmp_s,$tmp_m,$tmp_e);
	}
	
	private function Organize($f,$m,$s) {
		return (($f*588)+($m*28)+$s)+44032;
	}
	
	public function Trifle($utf8str) {
		$tmpStrg=array();
		$str_len=strlen($utf8str);
		if($bom_chk=(substr($utf8str,0,3)==="\xEF\xBB\xBF"))
			$utf8str=substr($utf8str,3,($str_len-=3));
		$i=0;
		while($i<$str_len) {
			$verint=ord(substr($utf8str,$i,1))>>3;
			if($verint>=0 && $verint<=15) {
				array_push($tmpStrg,ChoseongTrifler::UTF8CP(substr($utf8str,$i,1)));
				$i++;
			} elseif($verint>=24 && $verint<=27) {
				array_push($tmpStrg,ChoseongTrifler::UTF8CP(substr($utf8str,$i,2)));
				$i+=2;
			} elseif($verint>=28 && $verint<=29) {
				array_push($tmpStrg,ChoseongTrifler::UTF8CP(substr($utf8str,$i,3)));
				$i+=3;
			} elseif($verint===30) {
				array_push($tmpStrg,ChoseongTrifler::UTF8CP(substr($utf8str,$i,4)));
				$i+=4;
			}
		}
		
		$ConversionData[0][0]=array(1,11);
		$ConversionData[0][1]=array(2,11);
		$ConversionData[0][2]=array(4,11);
		$ConversionData[0][3]=array(7,11);
		$ConversionData[0][5]=array(8,11);
		$ConversionData[0][6]=array(16,11);
		$ConversionData[0][7]=array(17,11);
		$ConversionData[0][9]=array(19,11);
		$ConversionData[0][10]=array(20,11);
		$ConversionData[0][12]=array(22,11);
		$ConversionData[0][14]=array(23,11);
		$ConversionData[0][15]=array(24,11);
		$ConversionData[0][16]=array(25,11);
		$ConversionData[0][17]=array(26,11);
		$ConversionData[1][9]=array(3,11);
		$ConversionData[4][12]=array(5,11);
		$ConversionData[8][0]=array(9,11);
		$ConversionData[8][6]=array(10,11);
		$ConversionData[8][7]=array(11,11);
		$ConversionData[8][9]=array(12,11);
		$ConversionData[8][16]=array(13,11);
		$ConversionData[8][17]=array(14,11);
		$ConversionData[17][9]=array(18,11);
		
		$total=count($tmpStrg)-1;
		for($i=0;$i<$total;$i++) {
			$s1=$tmpStrg[$i];
			$s2=$tmpStrg[$i+1];
			
			if(($s1>=44032 && $s1<=55203) && ($s2>=44032 && $s2<=55203)) {
				$s1=ChoseongTrifler::Disorganize($s1);
				$s2=ChoseongTrifler::Disorganize($s2);
				if(empty($ConversionData[$s1[2]][$s2[0]][0])===false) {
					if(($s2[0]===3 || $s2[0]===16) && ($s2[1]===2 || $s2[1]===3 || $s2[1]===6 || $s2[1]===7 || $s2[1]===12 || $s2[1]===17 || $s2[1]===20))
						continue;
					$tmpStrg[$i]=ChoseongTrifler::Organize($s1[0],$s1[1],$ConversionData[$s1[2]][$s2[0]][0]);
					$tmpStrg[$i+1]=ChoseongTrifler::Organize($ConversionData[$s1[2]][$s2[0]][1],$s2[1],$s2[2]);
				}
			}
		}
		
		$rtnstr=$bom_chk?"\xEF\xBB\xBF":"";
		for($i=0;$i<($total)+1;$i++)
			$rtnstr.=ChoseongTrifler::CPUTF8($tmpStrg[$i]);
		
		return $rtnstr;
	}

}



$chan = $argv[1];
$ident = $argv[2];
$cmd = $argv[3];
$carg = $argv[4];

echo "meowbot\n";
if($carg == "")
{
	echo "사용법: !$cmd <초성 올려 쓰기를 적용할 글>\n";
}
else
{
	echo ChoseongTrifler::Trifle($carg) . "\n";
}





?>