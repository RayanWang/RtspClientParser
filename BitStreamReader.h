#pragma once


class CBitStreamReader
{
public:
	CBitStreamReader(unsigned char* dataToRead)
	{
		position = 0;
		m_pabinaryData =  dataToRead;
	}

	virtual ~CBitStreamReader(void)
	{
	}

	void SkipBits(int n)
	{
		for (int i = 0; i < n; i++)
		{
			SkipBit();
		}
	}

	int GetBits(int n) // Same as U(int n)
	{
		int result = 0;
		for (int i = 0; i < n; i++) 
		{
			result = result * 2 +GetBit();
		}

		return result;
	}

	int U(int n)
	{
		int result = 0;
		for (int i = 0; i < n; i++)
		{
			result = result * 2 +GetBit();
		}

		return result;
	}

	int Uev() 
	{
		return Ev(false);
	}

	int Sev()
	{
		return Ev(true);
	}

private:
	int GetBit()
	{
		int mask = 1 << (7 - (position & 7));
		int index = position >> 3;
		position++;
		return ((m_pabinaryData[index] & mask) == 0) ? 0 : 1;
	}

	void SkipBit()
	{
		position++;
	}

	int Ev(bool isItSigned)
	{
		int bitCount = 0;

		//std::string expGolomb;

		while (GetBit() == 0)
		{
			//expGolomb += '0';
			bitCount++;
		}

		//expGolomb += "/1";

		int result = 1;
		for (int i = 0; i < bitCount; i++)
		{
			int b = GetBit();
			//expGolomb += b;
			result = result * 2 + b;
		}

		result--;
		if (isItSigned) 
		{
			result = (result + 1) / 2 * (result % 2 == 0 ? -1 : 1);
		}
		return result;
	}

	unsigned char* m_pabinaryData;
	int position;
};