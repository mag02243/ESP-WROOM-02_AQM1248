<?php
$f = file('IPAGothic-31.bdf');
// Å‘å32x32
$H = 24;
$BHmax = 32;
$bmp0 = array(
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0);
$bmp = $bmp0;
$code = 0;
$w = 0;
$w0 = 0;
$bw = 0;
$bh = 0;
$ofsx = 0;
$ofsy = 0;
for($i = 0; $i < count($f); $i++) {
	if (substr($f[$i], 0, 9) == 'STARTCHAR') {
		$bmp = $bmp0;
		continue;
	}
	if (substr($f[$i], 0, 8) == 'ENCODING') {
		$code = intval(substr($f[$i], 9));
		$c = mb_chr($code, 'utf-16');
		if ($c == null)
			$c = chr($code);
		$u = mb_convert_encoding($c, 'utf-8', 'utf-16');
		if ($u == "")
			$u = $c;
//		var_dump($u);
		$n = strlen($u);
		$utf8 = '';
		for($j = 0; $j < $n; $j++)
			$utf8 .= sprintf('%02x', ord($u[$j]));
//		printf('//%04x %s %s<br>', $code, $u, $utf8);
		continue;
	}
	if (substr($f[$i], 0, 6) == 'DWIDTH') {
		sscanf(substr($f[$i], 6), '%d', $w);
		if ($w0 == 0)
			$w0 = $w;
		continue;
	}
	if (substr($f[$i], 0, 3) == 'BBX') {
		sscanf(substr($f[$i], 4), '%d %d %d %d', $bw, $bh, $ofsx, $ofsy);
		$y0 = $H - $bh - $ofsy;
		if ($y0 < 0)
			$y0 = 0;
		if ($y0 + $bh > $BHmax)
			$y0 = $BHmax - 1 - $bh;
//		printf('//%d %d %d %d > %d<br>', $bw, $bh, $ofsx, $ofsy, $y0);
		// BITMAP ‚ð”ò‚Î‚·
		$i++;
		for($j = 0; $j < $bh; $j++) {
			$i++;
			$o = 32 - $bw - $ofsx;
			$bmp[$y0 + $j] = intval(substr(trim($f[$i]) . '00000000', 0, 8), 16) >> $o;
//		printf("%s ", substr(trim($f[$i]) . '00000000', 0, 8));
		}
		continue;
	}
	if (substr($f[$i], 0, 7) == 'ENDCHAR') {
		$c = mb_chr($code, 'utf-16');
		if ($c == null)
			$c = chr($code);
		$u = mb_convert_encoding($c, 'utf-8', 'utf-16');
		if ($u == "")
			$u = $c;
		$n = strlen($u);
		$utf8 = '{ 0x';
		for($j = 0; $j < 3 - $n; $j++)
			$utf8 .= '00';
		for($j = 0; $j < $n; $j++)
			$utf8 .= sprintf('%02x', ord($u[$j]));
		$utf8 .= sprintf(', 0x%02x', $w);

		for($j = 0; $j < $BHmax; $j++)
			$utf8 .= sprintf(', 0x%08X', $bmp[$j]);
		$utf8 .= sprintf(" }, // '%s' %d %d<br>", $u, $bh, $ofsy);
		printf("%s", $utf8);
		$bmp = $bmp0;
		$code = 0;
		$w = $w0;
		$bw = 0;
		$bh = 0;
		$ofsx = 0;
		$ofsy = 0;
		continue;
	}
}
