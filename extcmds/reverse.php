#!/usr/bin/php
<?php

$chan = $argv[1];
$ident = $argv[2];
$cmd = $argv[3];
$carg = $argv[4];

echo "meowbot\n";
if($carg == "")
{
	echo "사용법: !$cmd <뒤집을 글>\n";
	exit(0);
}


$str = $carg;

$len = @mb_strlen($str, "utf-8");
for($i = $len; $i >= 0; $i--)
{
	$result .= @mb_substr($str, $i, 1, "utf-8");
}

echo "$result\n";

?>
