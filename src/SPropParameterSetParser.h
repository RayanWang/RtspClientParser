/*
 * Delta CONFIDENTIAL
 *
 * (C) Copyright Delta Electronics, Inc. 2014 All Rights Reserved
 *
 * NOTICE:  All information contained herein is, and remains the
 * property of Delta Electronics. The intellectual and technical
 * concepts contained herein are proprietary to Delta Electronics
 * and are protected by trade secret, patent law or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Delta Electronics.
 */

#pragma once

#include "BitStreamReader.h"

using namespace std;

static char g_Encoding_Table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};

void appendCharToCharArray(char* array, char a)
{
    size_t len = strlen(array);

    array[len] = a;
    array[len+1] = '\0';
}

template<class InputIterator, class OutputIterator>
OutputIterator copy (InputIterator first, InputIterator last, OutputIterator result)
{
	while (first != last) 
	{
		*result = *first;
		++result; 
		++first;
	}

	return result;
}

class CSPropParameterSetParser
{
public:
	CSPropParameterSetParser(const char* spsFromSdp)
	{
		this->spsFromSdp = spsFromSdp;
		m_pabinaryData = NULL;
		m_pbitStreamReader = NULL;
		m_pDecoding_Table = NULL;

		StartParsing();
	}

	int GetWidth()
	{
		int width  = (pic_width_in_mbs_minus1 + 1)*16;
		return width;
	}

	int GetHeight()
	{
		int height = (pic_height_in_map_units_minus1 + 1)*16;
		return  height;
	}

	virtual ~CSPropParameterSetParser(void)
	{
		if(m_pabinaryData)
		{
			delete [] m_pabinaryData;
			m_pabinaryData = NULL;
		}

		if(m_pbitStreamReader)
		{
			delete m_pbitStreamReader;
			m_pbitStreamReader = NULL;
		}

		if(m_pDecoding_Table)
			free(m_pDecoding_Table);
	}

private:
	void StartParsing()
	{
		ConvertFromBase64IntoByteArray();
		ParseSequenceParameterSet();
	}

	void ConvertFromBase64IntoByteArray()
	{
		size_t nOutputLen = 0;
		unsigned char* decodedString = DecodeBase64(spsFromSdp, strlen(spsFromSdp), &nOutputLen);

		m_pabinaryData = new unsigned char[nOutputLen];
		copy(decodedString, decodedString+nOutputLen-1, m_pabinaryData);
		free(decodedString);
	}

	void ParseSequenceParameterSet()
	{
		unsigned int temp;

		m_pbitStreamReader = new CBitStreamReader(m_pabinaryData);

		m_pbitStreamReader->U(8); // skip nal unit type

		profile_idc  =  m_pbitStreamReader->U(8);

		constraint_set0_flag = m_pbitStreamReader->U(1);
		constraint_set1_flag = m_pbitStreamReader->U(1);
		constraint_set2_flag = m_pbitStreamReader->U(1);
		constraint_set3_flag = m_pbitStreamReader->U(1);
		reserved_zero_4bits = m_pbitStreamReader->U(4);

		level_idc = m_pbitStreamReader->U(8);
		seq_parameter_set_id = m_pbitStreamReader->Uev();

		if (profile_idc  == 100 || profile_idc  == 110 ||
			profile_idc  == 122 || profile_idc  == 144)
		{
			chroma_format_idc = m_pbitStreamReader->Uev();

			if (chroma_format_idc == 3)
			{
				separate_colour_plane_flag = m_pbitStreamReader->U(1);
			}

			bit_depth_luma_minus8 = m_pbitStreamReader->Uev();
			bit_depth_chroma_minus8 = m_pbitStreamReader->Uev();
			qpprime_y_zero_transform_bypass_flag  = m_pbitStreamReader->U(1);
			seq_scaling_matrix_present_flag =  m_pbitStreamReader->U(1);

			if( seq_scaling_matrix_present_flag )
			{
				for(unsigned int ix = 0; ix < 8; ix++)
				{
					temp = m_pbitStreamReader->U(1);

					if (temp)
					{
						ScalingList(ix, ix < 6 ? 16 : 64);
					}
				}
			}
		}

		log2_max_frame_num_minus4 = m_pbitStreamReader->Uev();

		pic_order_cnt_type =  m_pbitStreamReader->Uev();

		if (pic_order_cnt_type == 0)
		{
			log2_max_pic_order_cnt_lsb_minus4 = m_pbitStreamReader->Uev();
		}
		else if (pic_order_cnt_type == 1)
		{
			delta_pic_order_always_zero_flag = m_pbitStreamReader->U(1);
			offset_for_non_ref_pic = m_pbitStreamReader->Sev();
			offset_for_top_to_bottom_field =  m_pbitStreamReader->Sev();

			num_ref_frames_in_pic_order_cnt_cycle = m_pbitStreamReader->Uev();

			for( int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
			{
				int skippedParameter = m_pbitStreamReader->Sev();
			}
		}

		num_ref_frames = m_pbitStreamReader->Uev();
		gaps_in_frame_num_value_allowed_flag = m_pbitStreamReader->U(1);

		pic_width_in_mbs_minus1 = m_pbitStreamReader->Uev();
		pic_height_in_map_units_minus1 = m_pbitStreamReader->Uev();

		frame_mbs_only_flag =  m_pbitStreamReader->U(1);

		if( !frame_mbs_only_flag )
		{
			mb_adaptive_frame_field_flag = m_pbitStreamReader->U(1);
		}

		direct_8x8_inference_flag =  m_pbitStreamReader->U(1);
		frame_cropping_flag = m_pbitStreamReader->U(1);

		if( frame_cropping_flag )
		{
			frame_crop_left_offset = m_pbitStreamReader->Uev();
			frame_crop_right_offset = m_pbitStreamReader->Uev();
			frame_crop_top_offset = m_pbitStreamReader->Uev();
			frame_crop_bottom_offset = m_pbitStreamReader->Uev();
		}

		vui_parameters_present_flag = m_pbitStreamReader->U(1);
	}

	// Utility to parse
	void ScalingList(unsigned int ix, unsigned int sizeOfScalingList)
	{
		unsigned int lastScale = 8;
		unsigned int nextScale = 8;
		unsigned int jx;
		int deltaScale;

		for (jx = 0; jx < sizeOfScalingList; jx++)
		{
			if (nextScale != 0)
			{
				deltaScale = m_pbitStreamReader->Sev();
				nextScale = (lastScale + deltaScale + 256) % 256;
			}
			if (nextScale != 0)
			{
				lastScale = nextScale;
			}
		}
	}

	void build_decoding_table() 
	{
		m_pDecoding_Table = (char*)malloc(256);

		for (int i = 0; i < 64; i++)
			m_pDecoding_Table[(unsigned char) g_Encoding_Table[i]] = i;
	}

	unsigned char *DecodeBase64(const char *data,
								size_t input_length,
								size_t *output_length)
	{
		if (m_pDecoding_Table == NULL) 
			build_decoding_table();

		*output_length = input_length / 4 * 3;
		if (data[input_length - 1] == '=') 
			(*output_length)--;
		if (data[input_length - 2] == '=') 
			(*output_length)--;

		unsigned char *decoded_data = (unsigned char*)malloc(*output_length);
		if (decoded_data == NULL) 
			return NULL;

		for (size_t i = 0, j = 0; i < input_length;) 
		{
			uint32_t sextet_a = data[i] == '=' ? 0 & i++ : m_pDecoding_Table[data[i++]];
			uint32_t sextet_b = data[i] == '=' ? 0 & i++ : m_pDecoding_Table[data[i++]];
			uint32_t sextet_c = data[i] == '=' ? 0 & i++ : m_pDecoding_Table[data[i++]];
			uint32_t sextet_d = data[i] == '=' ? 0 & i++ : m_pDecoding_Table[data[i++]];

			uint32_t triple = (sextet_a << 3 * 6)
								+ (sextet_b << 2 * 6)
								+ (sextet_c << 1 * 6)
								+ (sextet_d << 0 * 6);

			if (j < *output_length) 
				decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
			if (j < *output_length) 
				decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
			if (j < *output_length) 
				decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
		}

		return decoded_data;
	}

	bool IsBase64(unsigned char c)
	{
		return (isalnum(c) || (c == '+') || (c == '/'));
	}

	// Data to Parse
	unsigned char* m_pabinaryData;

	// Parameter from Describe RtspReponse: Base64 string
	const char* spsFromSdp;

	// Utility to read bits esaily
	CBitStreamReader* m_pbitStreamReader;

	// Parameters
	int profile_idc;
	int constraint_set0_flag;
	int constraint_set1_flag;
	int constraint_set2_flag;
	int constraint_set3_flag;
	int reserved_zero_4bits;
	int level_idc;
	int seq_parameter_set_id;
	int chroma_format_idc;
	int separate_colour_plane_flag;
	int bit_depth_luma_minus8;
	int bit_depth_chroma_minus8;
	int qpprime_y_zero_transform_bypass_flag;
	int seq_scaling_matrix_present_flag;
	int log2_max_frame_num_minus4;
	int pic_order_cnt_type;
	int log2_max_pic_order_cnt_lsb_minus4;
	int delta_pic_order_always_zero_flag;
	int offset_for_non_ref_pic;
	int offset_for_top_to_bottom_field;
	int num_ref_frames_in_pic_order_cnt_cycle;
	int num_ref_frames;
	int gaps_in_frame_num_value_allowed_flag;
	unsigned int  pic_width_in_mbs_minus1;
	unsigned int  pic_height_in_map_units_minus1;
	int frame_mbs_only_flag;
	int mb_adaptive_frame_field_flag;
	int direct_8x8_inference_flag;
	int frame_cropping_flag;
	int frame_crop_left_offset;
	int frame_crop_right_offset;
	int frame_crop_top_offset;
	int frame_crop_bottom_offset;
	int vui_parameters_present_flag;
	char *m_pDecoding_Table;
};

