<?php
$f = 'logo.png';
$sz = getimagesize($f);
$im = imagecreatefrompng($f);
$w = $sz[0];
$h = $sz[1];
printf("// w:%d h:%d<br>", $w, $h);
printf("const uint8_t logo_w = %d;<br>", $w);
printf("const uint8_t logo_h = %d;<br>", $h);
printf("const uint8_t logo[%d * %d] PROGMEM = {<br>", ($w + 7) >> 3, $h);
for($y = 0; $y < $h; $y++) {
	printf("  ");
	for($x = 0; $x < $w; $x += 8) {
		$b = 0;
		for($i = 0; $i < 8 && $x + $i < $w; $i++) {
			if (imagecolorat($im, $x + $i , $y) == 0)
				$b |= 1 << (7 - $i);
		}
		if ($x > 0)
			printf(", ");
		printf("0x%02x", $b);
	}
	if ($y < $h - 1)
		printf(",");
	printf("<br>");
}
printf("};");
