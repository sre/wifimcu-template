#include "FormatString.h"

#include <stdbool.h>
#include <limits.h>

#define FormatStringFloatingPointEnabled
#define FormatStringLongLongIntEnabled

#ifdef FormatStringLongLongIntEnabled
typedef long long int IntegerType;
typedef unsigned long long int UnsignedIntegerType;
#else
typedef long int IntegerType;
typedef unsigned long int UnsignedIntegerType;
#endif

static bool ParseFlags(const char **positiveprefix,bool *padleft,bool *padzero,
bool *alternate,const char **format);
static bool ParseSizes(int *width,int *precision,bool *padright,bool *padzero,const char **format,va_list args);
static int ParseInteger(const char **format,va_list args);
static bool ParseLengthModifier(int *modifier,const char **format);

static int OutputString(FormatOutputFunction *outputfunc,void *context,const char *string,
bool padright,int padzero,int width,int precision);

static int OutputSignedInteger(FormatOutputFunction *outputfunc,void *context,
IntegerType value,const char *positiveprefix,bool padright,bool padzero,int width,int precision);
static int OutputUnsignedInteger(FormatOutputFunction *outputfunc,void *context,
UnsignedIntegerType value,unsigned int base,char firstletter,const char *prefix,
bool padright,bool padzero,int width,int precision);

#ifdef FormatStringFloatingPointEnabled
static int OutputFloatingPoint(FormatOutputFunction *outputfunc,void *context,
double value,const char *positiveprefix,bool padright,bool padzero,
bool forceperiod,bool uppercase,int width,int precision);
static int ConvertFloatingPoint(char *buffer,double absvalue,
int precision,bool forceperiod,bool uppercase,bool *isinf,bool *isnan);
#endif

static int OutputDigitString(FormatOutputFunction *outputfunc,void *context,
const char *prefix,int numextrazeroes,const char *digitstring,int numdigits,bool padright,int width);

int FormatString(FormatOutputFunction *outputfunc,void *context,const char *format,va_list args)
{
	int count=0;

	while(*format)
	{
		if(*format=='%')
		{
			format++;

			// Check if we just need to output a literal %.
			if(*format=='%')
			{
				outputfunc('%',context);
				format++;
				count++;
				continue;
			}

			const char *positiveprefix;
			bool padright,padzero,alternate;
			if(!ParseFlags(&positiveprefix,&padright,&padzero,&alternate,&format)) break;

			int width,precision;
			if(!ParseSizes(&width,&precision,&padright,&padzero,&format,args)) break;

			int modifier;
			if(!ParseLengthModifier(&modifier,&format)) break;

			// Parse the conversion specifier and print appropriate output.
			switch(*format)
			{
				case 's':
				{
					char *string=va_arg(args,char *);
					count+=OutputString(outputfunc,context,string?string:"(null)",
					padright,padzero,width,precision);
				}
				break;

				case 'c':
				{
					char c=va_arg(args,int);
					char string[2]={ c,0 };
					count+=OutputString(outputfunc,context,string?string:"(null)",
					padright,padzero,width,-1);
				}
				break;

				case 'd':
				case 'i':
				{
					IntegerType value;
					switch(modifier)
					{
						default: value=va_arg(args,int); break;
						case 1: value=va_arg(args,long int); break;
						#ifdef FormatStringLongLongIntEnabled
						case 2: value=va_arg(args,long long int); break;
						#endif
						case -1: value=(short)va_arg(args,int); break;
						case -2: value=(signed char)va_arg(args,int); break;
					}

					OutputSignedInteger(outputfunc,context,value,
					positiveprefix,padright,padzero,width,precision);
				}
				break;

				case 'u':
				case 'o':
				case 'x':
				case 'X':
				{
					UnsignedIntegerType value;
					switch(modifier)
					{
						default: value=va_arg(args,unsigned int); break;
						case 1: value=va_arg(args,unsigned long int); break;
						#ifdef FormatStringLongLongIntEnabled
						case 2: value=va_arg(args,unsigned long long int); break;
						#endif
						case -1: value=(unsigned short)va_arg(args,unsigned int); break;
						case -2: value=(unsigned char)va_arg(args,unsigned int); break;
					}

					switch(*format)
					{
						case 'u':
							OutputUnsignedInteger(outputfunc,context,value,10,0,
							0,padright,padzero,width,precision);
						break;

						case 'o':
							OutputUnsignedInteger(outputfunc,context,value,8,0,
							alternate&&(value!=0||precision==0)?"0":0,padright,padzero,width,precision);
						break;

						case 'x':
							OutputUnsignedInteger(outputfunc,context,value,16,'a',
							alternate&&value!=0?"0x":0,padright,padzero,width,precision);
						break;

						case 'X':
							OutputUnsignedInteger(outputfunc,context,value,16,'A',
							alternate&&value!=0?"0X":0,padright,padzero,width,precision);
						break;
					}
				}
				break;

				#ifdef FormatStringFloatingPointEnabled
				case 'f':
				case 'F':
				{
					double value=va_arg(args,double); // TODO: long double?
					OutputFloatingPoint(outputfunc,context,value,
					positiveprefix,padright,padzero,alternate,*format=='F',width,precision);
				}
				break;
				#endif
			}

			format++;
		}
		else
		{
			outputfunc(*format,context);
			format++;
			count++;
		}
	}
	return count;
}

static bool ParseFlags(const char **positiveprefix,bool *padright,bool *padzero,
bool *alternate,const char **format)
{
	*positiveprefix=0;
	*padright=false;
	*padzero=false;
	*alternate=false;

	for(;;)
	{
		switch(**format)
		{
			case '#': // Alternate form.
				*alternate=true;
			break;

			case '-': // Left justify. Overrides zero padding.
				*padright=true;
				*padzero=false;
			break;

			case '0': // Zero padding.
				if(!*padright) *padzero=true; // Overriden by left justification, so check for that.
			break;

			case '+': // Prefix positive numbers with a plus sign. Overrides space prefix.
				*positiveprefix="+";
			break;

			case ' ': // Prefix positive numbers with a space.
				if(!*positiveprefix) *positiveprefix=" "; // Overriden by plus prefix, so check for that.
			break;

			case 0: return false;

			default: return true;
		}

		(*format)++;
	}
}

static bool ParseSizes(int *width,int *precision,bool *padright,bool *padzero,const char **format,va_list args)
{
	// Parse field width, if any.
	*width=ParseInteger(format,args);
	if(*width<0) // If a width supplied to a * parameter is negative, act as if the - flag had been used.
	{
		*width=-*width;
		*padright=true;
		*padzero=false;
	}

	// Check for and parse precision.
	if(**format=='.')
	{
		(*format)++;
		*precision=ParseInteger(format,args);
	}
	else
	{
		*precision=-1;
	}

	return **format!=0;
}

static int ParseInteger(const char **format,va_list args)
{
	if(**format=='*')
	{
		(*format)++;
		return va_arg(args,int);
	}
	else
	{
		int value=0;
		while(**format>='0' && **format<='9')
		{
			value*=10;
			value+=**format-'0';
			(*format)++;
		}
		return value;
	}
}

static bool ParseLengthModifier(int *modifier,const char **format)
{
	// TODO: More modifiers.

	*modifier=0;

	if(**format=='l')
	{
		*modifier=1;
		(*format)++;
		if(**format=='l')
		{
			*modifier=2;
			(*format)++;
		}
	}
	else if(**format=='h')
	{
		*modifier=-1;
		(*format)++;
		if(**format=='h')
		{
			*modifier=-2;
			(*format)++;
		}
	}

	return **format!=0;
}




static int OutputString(FormatOutputFunction *outputfunc,void *context,const char *string,
bool padright,int padzero,int width,int precision)
{
	int count=0;

	// If we need to pad on the left, calculate the length of the string and
	// print the appropriate amount of padding.
	if(width>0 && !padright)
	{
		const char *end=string;
		while(*end) end++;

		int length=(int)(end-string);
		if(precision>=0 && length>precision) length=precision;

		for(int i=0;i<width-length;i++)
		{
			outputfunc(padzero?'0':' ',context);
			count++;
		}
	}

	// Print the contents of the string, stopping after precision characters if precision is set.
	const char *stringend=0;
	if(precision>=0) stringend=string+precision;
	while(*string && string!=stringend)
	{
		outputfunc(*string,context);
		count++;
		string++;
	}

	// Print any remaining padding on the right, if needed.
	while(count<width)
	{
		outputfunc(' ',context);
		count++;
	}

	return count;
}




static int OutputSignedInteger(FormatOutputFunction *outputfunc,void *context,
IntegerType value,const char *positiveprefix,bool padright,bool padzero,int width,int precision)
{
	// Check if the number is signed and negative.
	UnsignedIntegerType absvalue;
	const char *prefix;
	if(value>=0)
	{
		absvalue=value;
		prefix=positiveprefix;
	}
	else
	{
		absvalue=-value;
		prefix="-";
	}

	return OutputUnsignedInteger(outputfunc,context,absvalue,10,0,prefix,padright,padzero,width,precision);
}

static int OutputUnsignedInteger(FormatOutputFunction *outputfunc,void *context,
UnsignedIntegerType value,unsigned int base,char firstletter,const char *prefix,
bool padright,bool padzero,int width,int precision)
{
	// Generate the digits in reverse.
	char buffer[21];
	char *bufferend=&buffer[sizeof(buffer)];
	char *digitstring=bufferend;

	if(value==0)
	{
		if(precision!=0) *--digitstring='0';
	}
	else
	{
		while(value)
		{
			unsigned int digit=value%base;

			char c;
			if(digit>=10) c=digit-10+firstletter;
			else c=digit+'0';

			*--digitstring=c;

			value/=base;
		}
	}

	// Calculate the number of digits actually generated.
	int numdigits=(int)(bufferend-digitstring);

	// Calculate the number of extra zeroes to add.
	int numextrazeroes;
	if(padzero && precision<0)
	{
		// If zero padding has been requested, and no precision has been set,
		// leave it to OutputDigitString to zero-pad.
		numextrazeroes=-1;
	}
	else
	{
		// Otherwise, add enough zeroes to match the precision
		numextrazeroes=precision-numdigits;
		// Special case: octal uses a "0" prefix, which counts as a zero, so add one less zero.
		if(prefix && prefix[0]=='0' && prefix[1]==0) numextrazeroes--;

		if(numextrazeroes<0) numextrazeroes=0;
	}

	return OutputDigitString(outputfunc,context,prefix,numextrazeroes,digitstring,numdigits,padright,width);
}



#ifdef FormatStringFloatingPointEnabled

#include <math.h>

static int OutputFloatingPoint(FormatOutputFunction *outputfunc,void *context,
double value,const char *positiveprefix,bool padright,bool padzero,
bool forceperiod,bool uppercase,int width,int precision)
{
	// Check if the number is signed and negative.
	double absvalue;
	const char *prefix;
	if(copysign(1,value)>=0)
	{
		absvalue=value;
		prefix=positiveprefix;
	}
	else
	{
		absvalue=-value;
		prefix="-";
	}

	// Default precision is 6.
	if(precision<0) precision=6;

	// Maximum precision is 100, to avoid overflowing the buffer.
	if(precision>100) precision=100;

	// Set up zero padding if needed.
	int numextrazeroes;
	if(padzero) numextrazeroes=-1; // Let OutputDigitString pad with zeroes.
	else numextrazeroes=0;

	// Convert the number to a string, and count the number of digits.
	char buffer[128];
	bool isinf,isnan;
	int numdigits=ConvertFloatingPoint(buffer,absvalue,precision,forceperiod,uppercase,&isinf,&isnan);

	if(isinf || isnan) numextrazeroes=0;
	if(isnan) prefix=0;

	return OutputDigitString(outputfunc,context,prefix,numextrazeroes,buffer,numdigits,padright,width);
}

static int ConvertFloatingPoint(char *buffer,double absvalue,
int precision,bool forceperiod,bool uppercase,bool *isinf,bool *isnan)
{
	absvalue+=0.5/pow(10,precision);

	union { unsigned long long l; double f; } pun;

	pun.f=absvalue;
	int exp2=((pun.l>>52)&0x7ff)-1023;
	unsigned long long mant=pun.l&0x000fffffffffffffULL;

	//if((*sign=x.l>>63)) value=-value;

	*isnan=*isinf=false;

	if(exp2==0x400)
	{
		if(mant) { buffer[0]='N'; buffer[1]='A'; buffer[2]='N'; *isnan=true; }
		else { buffer[0]='I'; buffer[1]='N'; buffer[2]='F'; *isinf=true; }
		//buffer[3]=0;

		if(!uppercase) for(int i=0;i<3;i++) buffer[i]+='a'-'A';

		return 3;
	}

	// Find base-10 exponent.
//	int exp10=(absvalue==0)?!fflag:(int)ceil(log10(absvalue));
	int exp10=(int)ceil(log10(absvalue));
	if(exp10<-307) exp10=-307; // Avoid overflow in pow()

	// Attempt to scale value to 0.1<=x<1.0.
	absvalue*=pow(10.0,-exp10);

	// Correct scaling if it went wrong.
	if(absvalue)
	{
		while(absvalue<0.1) { absvalue*=10; exp10--; }
		while(absvalue>=1.0) { absvalue/=10; exp10++; }
	}

	// Calculate the numbers and positions to print.
	int numberofdigits,firstdigit,decimalpoint;
	if(exp10>0)
	{
		numberofdigits=exp10+precision+1;
		firstdigit=0;
		decimalpoint=exp10;
	}
	else
	{
		numberofdigits=precision+2;
		firstdigit=-exp10+2;
		decimalpoint=1;

	}

	// Elide decimal point for 0-precision, except when explicitly requested.
	if(precision==0)
	{
		if(!forceperiod) numberofdigits--;
	}

	pun.f=absvalue;
	exp2=((pun.l>>52)&0x7ff)-1023;
	mant=pun.l&0x000fffffffffffffULL;

	if(exp2==-1023) exp2++;
	else mant|=0x0010000000000000ULL;

	// TODO: What is this supposed to do?
	mant<<=(exp2+4); // 56-bit denormalised signifier

	char *ptr=buffer;

	for(int i=0;i<numberofdigits;i++)
	{
		if(i==decimalpoint)
		{
			*ptr++='.';
		}
		else if(i<firstdigit)
		{
			*ptr++='0';
		}
		else
		{
			mant&=0x00ffffffffffffffULL; // mod 1.0
			mant*=10;
			*ptr++=(mant>>56)+'0';
		}
	}

	//*ptr=0;

	return (int)(ptr-buffer);
}

#endif

static int OutputDigitString(FormatOutputFunction *outputfunc,void *context,
const char *prefix,int numextrazeroes,const char *digitstring,int numdigits,bool padright,int width)
{
	// Check length of prefix (only 0, 1 or 2).
	int numprefix;
	if(!prefix || prefix[0]==0) numprefix=0;
	else if(prefix[1]==0) numprefix=1;
	else numprefix=2;

	// Negative numextrazeroes means to pad with zeroes. Calculate how many are needed.
	if(numextrazeroes<0)
	{
		numextrazeroes=width-numdigits-numprefix;
		if(numextrazeroes<0) numextrazeroes=0;
	}

	// Calculate total number of characters without space padding.
	int numcharacters=numdigits+numextrazeroes+numprefix;

	// Calculate number of spaces for padding.
	int numpadding=width-numcharacters;
	if(numpadding<0) numpadding=0;

	// Start actually outputting.

	// If we are not padding on the right, output space padding first.
	if(!padright)
	{
		for(int i=0;i<numpadding;i++) outputfunc(' ',context);
	}

	// Output a prefix, if supplied.
	for(int i=0;i<numprefix;i++) outputfunc(prefix[i],context);

	// Output extra zeroes.
	for(int i=0;i<numextrazeroes;i++) outputfunc('0',context);

	// Output the actual string of digits.
	for(int i=0;i<numdigits;i++) outputfunc(digitstring[i],context);

	// If we are padding on the right, output space padding last.
	if(padright)
	{
		for(int i=0;i<numpadding;i++) outputfunc(' ',context);
	}

	return numcharacters+numpadding;
}




static void SprintfOutputFunction(char c,void *context)
{
	char **stringptr=context;
	**stringptr=c;
	(*stringptr)++;
}

int sprintf(char *buffer,const char *format,...)
{
	va_list args;
	va_start(args,format);
	int val=FormatString(SprintfOutputFunction,&buffer,format,args);
	va_end(args);

	*buffer=0;

	return val;
	
}
