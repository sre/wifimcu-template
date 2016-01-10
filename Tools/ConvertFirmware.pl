#!/usr/bin/perl

use strict;

binmode STDIN;

my $data;
$data.=$_ while(<STDIN>);

my $size=length $data;

print "#include \"wiced_resource.h\"\n";
print "\n";
print "const char wifi_firmware_image_data[$size]=\n";
print "{";

foreach my $i (0..$size-1)
{
	print "\n\t" if $i%16==0;
	printf "0x%02x,",ord(substr($data,$i,1));
}

print "\n};\n";
print "\n";


print "const resource_hnd_t wifi_firmware_image=\n";
print "{\n";
print "	RESOURCE_IN_MEMORY,$size,{ .mem={ wifi_firmware_image_data } }\n";
print "};\n";

