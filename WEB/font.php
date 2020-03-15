<?php
$f = file('k8x12.bdf');
$bmp0 = array(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
$bmp = $bmp0;
$code = 0;
$w = 0;
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
		continue;
	}
	if (substr($f[$i], 0, 3) == 'BBX') {
		sscanf(substr($f[$i], 4), '%d %d %d %d', $bw, $bh, $ofsx, $ofsy);
		$y0 = 11 - $bh - $ofsy;
		if ($y0 < 0)
			$y0 = 0;
		if ($y0 + $bh > 12)
			$y0 = 0;
//		printf('//%d %d %d %d > %d<br>', $bw, $bh, $ofsx, $ofsy, $y0);
		// BITMAP ‚ð”ò‚Î‚·
		$i++;
		for($j = 0; $j < $bh; $j++) {
			$i++;
			$bmp[$y0 + $j] = intval($f[$i], 16) >> $ofsx;
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
		$utf8 = '{ ';
		for($j = 0; $j < 3 - $n; $j++) {
			if ($j > 0)
				$utf8 .= ', ';
			$utf8 .= '0x00';
		}
		for($j = 0; $j < $n; $j++) {
			if (!($n == 3 && $j == 0))
				$utf8 .= ', ';
			$utf8 .= sprintf('0x%02x', ord($u[$j]));
		}
		$utf8 .= sprintf(', 0x%02x', $w);

		for($j = 0; $j < 12; $j++)
			$utf8 .= sprintf(', 0x%02X', $bmp[$j]);
		$utf8 .= sprintf(" }, // '%s'<br>", $u);
		printf("%s", $utf8);
		$bmp = $bmp0;
		$code = 0;
		$bw = 0;
		$bh = 0;
		$ofsx = 0;
		$ofsy = 0;
		continue;
	}
}
