//============================================================================
// Name        : mpeg.cpp
// Author      : hanzhongtao
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <vector>

using namespace std;

typedef struct TS_packet_header
{
	unsigned char sync_byte;
	unsigned transport_error_indicator;
	unsigned payload_unit_start_indicator;
	unsigned transport_priority;
	unsigned PID;
	unsigned transport_scrambling_control;
	unsigned adaption_field_control;
	unsigned continuity_counter;

	void print()
	{
		printf("sync_byte = %x,transport_error_indicator = %d,payload_unit_start_indicator = %d,transport_priority = %d, PID = %d,transport_scrambling_control = %d,adaption_field_control = %d, continuity_counter = %d \n",
	           sync_byte,transport_error_indicator,payload_unit_start_indicator,transport_priority,PID,transport_scrambling_control,adaption_field_control,continuity_counter);

	};
} TS_packet_header;


void adjust_TS_packet_header(TS_packet_header* pheader,unsigned char * buf)
{
	pheader->sync_byte = buf[0];
	pheader->transport_error_indicator = buf[1] >> 7;
	pheader->payload_unit_start_indicator = buf[1] >> 6 & 0x01;
	pheader->transport_priority = buf[1] >> 5 & 0x01;
	pheader->PID = (buf[1] & 0x1F) << 8 | buf[2];
	pheader->transport_scrambling_control = buf[3] >> 6;
	pheader->adaption_field_control = buf[3] >> 4 & 0x03;
	pheader->continuity_counter = buf[3] & 0xf;
}

typedef struct TS_PAT
{
	unsigned table_id;
	unsigned section_syntax_indicator;
	unsigned zero;
	unsigned reserved_1;
	unsigned section_length;
	unsigned transport_stream_id;
	unsigned reserved_2;
	unsigned version_number;
	unsigned current_next_indicator;
	unsigned section_number;
	unsigned last_section_number;
	unsigned program_number;
	unsigned reserved_3;
	unsigned network_PID;
	unsigned program_map_PID;
	unsigned CRC_32;
} TS_PAT;




void adjust_PAT_table ( TS_PAT * packet, unsigned char * buffer )
{

	int n = 0;
	int i = 0;

	int len = 0;
	packet->table_id = buffer[0];
	packet->section_syntax_indicator= buffer[1] >> 7& 0x1;
	packet->zero= buffer[1] >> 6 & 0x1;
	packet->reserved_1= buffer[1] >> 4 & 0x3;
	packet->section_length= (buffer[1] & 0x0F) << 8 | buffer[2];
	packet->transport_stream_id= buffer[3] << 8 | buffer[4];
	packet->reserved_2= buffer[5] >> 6 & 0x3;

	packet->version_number =buffer[5] >> 1 & 0x1F;

	packet->current_next_indicator = (buffer[5] << 7) >> 7 &0x1;
	packet->section_number= buffer[6];
	packet->last_section_number = buffer[7];

 // Get CRC_32
	len = 3 + packet->section_length;
	packet->CRC_32= (buffer[len-4] & 0x000000FF) << 24
	| (buffer[len-3] & 0x000000FF) << 16
	| (buffer[len-2] & 0x000000FF) << 8
	| (buffer[len-1] & 0x000000FF);
	// Parse network_PID or program_map_PID
	for ( n = 0; n < packet->section_length - 4; n ++ )
	{
	packet->program_number= buffer[8] << 8 | buffer[9];
	packet->reserved_3= buffer[10] >> 5 & 0x7;
	if ( packet->program_number == 0x0 )
		packet->network_PID = (buffer[10] << 3) << 5 | buffer[11];
	else
	{
		//packet->program_map_PID = (buffer[10] << 3) << 5 | buffer[11];
		packet->program_map_PID = (buffer[10] &0x1f) << 8 | buffer[11];
	}
	n += 5;
	}
}


typedef struct TS_PMT
{
unsigned table_id;
unsigned section_syntax_indicator;
unsigned zero;
unsigned reserved_1;
unsigned section_length;
unsigned program_number;
unsigned reserved_2;
unsigned version_number;
unsigned current_next_indicator;
unsigned section_number;
unsigned last_section_number;
unsigned reserved_3;
unsigned PCR_PID;
unsigned reserved_4;
unsigned program_info_length;

unsigned stream_type;
unsigned reserved_5;
unsigned elementary_PID;
unsigned reserved_6;
unsigned ES_info_length;
unsigned CRC_32;
} TS_PMT;


typedef struct TS_ES
{
unsigned type;
unsigned pid;

TS_ES()
{
	type = -1;
	pid = -1;
}
} TS_ES;

TS_ES g_es[2];


void adjust_PMT_table ( TS_PMT * packet, unsigned char * buffer )
{
	int pos = 12, len = 0;
	int i = 0;
	packet->table_id = buffer[0];
	packet->section_syntax_indicator = buffer[1] >> 7& 0x1;
	packet->zero= buffer[1] >> 6& 0x1;
	packet->reserved_1= buffer[1] >> 4 & 0x3;
	packet->section_length= (buffer[1] & 0x0F) << 8 | buffer[2];
	packet->program_number= buffer[3] << 8 | buffer[4];
	packet->reserved_2= buffer[5] >> 6& 0x3;
	packet->version_number= buffer[5] >> 1 & 0x1F;
	packet->current_next_indicator= (buffer[5] << 7) >> 7;
	packet->section_number= buffer[6];
	packet->last_section_number= buffer[7];
	packet->reserved_3= buffer[8] >> 5& 0x7;
	packet->PCR_PID= ((buffer[8] << 8) | buffer[9]) & 0x1FFF;
	packet->reserved_4= buffer[10] >> 4;
	packet->program_info_length= (buffer[10] & 0x0F) << 8 | buffer[11];
	// Get CRC_32
	len = packet->section_length + 3;
	packet->CRC_32= (buffer[len-4] & 0x000000FF) << 24
			| (buffer[len-3] & 0x000000FF) << 16
			| (buffer[len-2] & 0x000000FF) << 8
			| (buffer[len-1] & 0x000000FF);
	// program info descriptor
	if ( packet->program_info_length != 0 )
		pos += packet->program_info_length;
	// Get stream type and PID   
	for ( ; pos <= (packet->section_length + 2 ) - 4; )
	{
		packet->stream_type= buffer[pos];
		packet->reserved_5= buffer[pos+1] >> 5;
		packet->elementary_PID= ((buffer[pos+1] << 8) | buffer[pos+2]) & 0x1FFF;
		packet->reserved_6= buffer[pos+3] >> 4;
		packet->ES_info_length= (buffer[pos+3] & 0x0F) << 8 | buffer[pos+4];
		// Store in es
		g_es[i].type = packet->stream_type;
		g_es[i].pid = packet->elementary_PID;
		if ( packet->ES_info_length != 0 )
		{
			pos = pos+5;
			pos += packet->ES_info_length;
		}
		else
		{
			pos += 5;
		}
		i++;
}
}


typedef struct TS_AF
{
	unsigned adaptation_fild_length;
	unsigned discontinuity_indicator;
	unsigned random_access_indicator;
	unsigned elementary_stream_priority_indicator;
	unsigned PCR_flag;
	unsigned OPCR_flag;
	unsigned splicing_point_flag;
	unsigned transport_private_data_flag;
	unsigned adaptation_field_extension_flag;

	unsigned program_clock_reference_base;
	unsigned reserved1;
	unsigned program_clock_reference_extension;

} TS_AF;


void adjust_Adaptation_field ( TS_AF * packet, unsigned char * buffer )
{
	packet->adaptation_fild_length = buffer[0];

	if(packet->adaptation_fild_length > 0 )
	{
		packet->discontinuity_indicator = buffer[1] >> 7& 0x1;
		packet->random_access_indicator = buffer[1] >> 6& 0x1;
		packet->elementary_stream_priority_indicator= buffer[1] >> 5 & 0x1;
		packet->PCR_flag= buffer[1] >> 4 &0x1;
		packet->OPCR_flag= buffer[1] >> 3 &0x1;
		packet->splicing_point_flag= buffer[1] >> 2 &0x1;
		packet->transport_private_data_flag= buffer[1] >> 1 & 0x1;
		packet->adaptation_field_extension_flag= buffer[1] & 0x1;
	}


	if(packet->PCR_flag)
	{
		packet->program_clock_reference_base |= buffer[2] << 25 ;
		packet->program_clock_reference_base |= buffer[3] << 17;
		packet->program_clock_reference_base |= buffer[4] << 9 ;
		packet->program_clock_reference_base |= buffer[5] <<1 ;
		packet->program_clock_reference_base |= buffer[6]>> 7 & 0x1;
		packet->reserved1 = buffer[6] >>1 & 0x3f;
		packet->program_clock_reference_extension = (buffer[6] & 0x1)<<8 | buffer[7];
	}
}


typedef struct TS_PES_H
{
	unsigned packet_start_code_prefix;
	unsigned stream_id;
	unsigned PES_packet_length;
	unsigned flag;
	unsigned PES_scrambling_control;
	unsigned PES_priority;
	unsigned data_alignmet_indicator;
	unsigned copyright;
	unsigned original_or_copy;
	unsigned PTS_DTS_flags;
	unsigned ESCR_flag;
	unsigned ESCR_rate_flag;
	unsigned DSM_trick_mode_flag;
	unsigned additional_copy_info_flag;
	unsigned PES_CRC_flag;
	unsigned PES_extension_flag;
	unsigned PES_header_dada_length;
	unsigned PTS;
	unsigned DTS;

} TS_PES_H;


void adjust_pes_header ( TS_PES_H * packet, unsigned char * buffer )
{
	packet->packet_start_code_prefix = buffer[0]<<16 | buffer[1] << 8 | buffer[2];
	packet->stream_id = buffer[3];
	packet->PES_packet_length = buffer[4] << 8 |  buffer[5];

	if((packet->stream_id >> 4 )== 0xe)
	{
		packet->flag= buffer[6] >> 6 & 0x3;//'10'
		packet->PES_scrambling_control= buffer[6] >> 4 &0x3;
		packet->PES_priority= buffer[6] >> 3 &0x1;
		packet->data_alignmet_indicator= buffer[6] >> 2 &0x1;
		packet->copyright= buffer[6] >> 1 & 0x1;
		packet->original_or_copy= buffer[6] & 0x1;

		packet->PTS_DTS_flags = buffer[7] >>6 & 0x3;
		packet->ESCR_flag                  = buffer[7]>> 5 & 0x1;
		packet->ESCR_rate_flag             = buffer[7]>> 4 & 0x1;
		packet->DSM_trick_mode_flag        = buffer[7]>> 3 & 0x1;
		packet->additional_copy_info_flag  = buffer[7]>> 2 & 0x1;
		packet->PES_CRC_flag               = buffer[7]>> 1 & 0x1;
		packet->PES_extension_flag         = buffer[7]     & 0x1;
		packet->PES_header_dada_length     = buffer[8];

		if(packet->PTS_DTS_flags == 0x3)
		{
			int flag = buffer[9]>>4 &0xf; //'0011'
			packet->PTS = 0;
			packet->PTS |=(buffer[9]>>1& 0x7)<<30;
			packet->PTS |=buffer[10]<<22;
			packet->PTS |=(buffer[11]>>1&0x7f) << 15;
			packet->PTS |=buffer[12]<<7;
			packet->PTS |=(buffer[13]>>1&0x7f);

			int flag2 = buffer[14]>>4 &0xf; //'0001'
			packet->DTS  = 0;
			packet->DTS |=(buffer[14]>>1& 0x7)<<30;
			packet->DTS |=buffer[15]<<22;
			packet->DTS |=(buffer[16]>>1&0x7f) << 15;
			packet->DTS |=buffer[17]<<7;
			packet->DTS |=(buffer[18]>>1&0x7f);
		}

	}


/*
	packet->program_clock_reference_base = 0;

	packet->program_clock_reference_base |= buffer[2] << 25 ;
	packet->program_clock_reference_base |= buffer[3] << 17;

	packet->program_clock_reference_base |= buffer[4] << 9 ;
	packet->program_clock_reference_base |= buffer[5] <<1 ;
	packet->program_clock_reference_base |= buffer[6]>> 7 & 0x1;
	//packet->program_clock_reference_base = 0;
	//packet->program_clock_reference_base = ( buffer[4] << 8 | buffer[5] );
	packet->reserved1 = buffer[6] >>1 & 0x3f;
	packet->program_clock_reference_extension = (buffer[6] & 0x1)<<8 | buffer[7];
	*/
}


typedef struct TS_SEQUENCE_HEADER
{
	unsigned sequence_header_code;
	unsigned horizontal_size_value;
	unsigned vertical_size_value;
	unsigned aspect_ratio_information;//4bit
	unsigned frame_rate_code;//4bit;
	unsigned bit_rate_value;//18 bit
	unsigned marker_bit;//1bit
	unsigned vbv_buffer_size_value;//10bit
	unsigned constrained_parameters_flag;// 1bit
	unsigned load_intra_quantiser_matrix;// 1bit


} TS_SEQUENCE_HEADER;



void adjust_sequence_header ( TS_SEQUENCE_HEADER * packet, unsigned char * buffer )
{
	packet->sequence_header_code = buffer[0]<<24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
	packet->horizontal_size_value = buffer[4]<<4 | buffer[5] >> 4 & 0xf;
	packet->vertical_size_value = (buffer[5] &0xf ) << 8 | buffer[6];
	packet->aspect_ratio_information = buffer[7]>>4 & 0xf;
	packet->frame_rate_code = buffer[7]& 0xf;
	packet->bit_rate_value = 0;
	packet->bit_rate_value = buffer[8] << 10 | buffer[9] << 2 | (buffer[10]>> 6 & 0x3);
	packet->marker_bit = buffer[10]>>5 &0x1;
	packet->vbv_buffer_size_value = (buffer[10] &0x1f)<<5 | buffer[11]>>3 & 0x1f;
	packet->constrained_parameters_flag =  buffer[11]>>2&0x1;
	packet->load_intra_quantiser_matrix = buffer[11]>>1&0x1;
}


typedef struct TS_SEQUENCE_EXTENSION
{
	unsigned extension_start_code;//32bit
	unsigned extension_start_code_identifier;//4bit
	unsigned profile_and_level_indication;//8bit
	unsigned progressive_sequence;//1bit
	unsigned chroma_format;//2bit
	unsigned horizontal_size_extension;//2bit
	unsigned vertical_size_extension;//2bit
	unsigned bit_rate_extension;//12bit
	unsigned marker_bit;//1bit
	unsigned vbv_buffer_size_extension;//8bit
	unsigned low_delay;//1bit
	unsigned frame_rate_extension_n;//2bit
	unsigned frame_rate_extension_d;//5bit
} TS_SEQUENCE_EXTENSION;


void adjust_sequence_extension ( TS_SEQUENCE_EXTENSION * packet, unsigned char * buffer )
{
	packet->extension_start_code = buffer[0]<<24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
	packet->extension_start_code_identifier = buffer[4] >> 4 & 0xf;
	packet->profile_and_level_indication = (buffer[4] &0xf ) << 4 | buffer[5]>>4 & 0xf;
	packet->progressive_sequence = buffer[5]>>3 & 0x1;
	packet->chroma_format = buffer[5]>>1& 0x3;
	packet->horizontal_size_extension = (buffer[5] & 0x1)<<1 | (buffer[6]>>7 & 0x1);
	packet->vertical_size_extension = buffer[6]>> 5 & 0x3;
	packet->bit_rate_extension = buffer[6]&0x1f <<7 |buffer[7] & 0x7f;

	packet->marker_bit = buffer[7] & 0x1f;
	packet->vbv_buffer_size_extension =  buffer[8];
	packet->low_delay = buffer[9]>>7&0x1;
	packet->frame_rate_extension_n = buffer[9]>>5&0x3;
	packet->frame_rate_extension_d = buffer[9]&0x1f;
}
typedef struct TS_GROUP_OF_PICTURES
{
	unsigned group_start_code;//32bit;
	unsigned time_code;//25bit;
	unsigned closed_gop;//1bit;
	unsigned broken_link;//1bit;
} TS_GROUP_OF_PICTURES;


void adjust_group_of_pictures( TS_GROUP_OF_PICTURES * packet, unsigned char * buffer )
{
	packet->group_start_code = buffer[0]<<24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
	packet->time_code = buffer[4] << 17 | buffer[5] << 9 || buffer[6] << 1 | (buffer[7] >> 7 & 0x1);
	packet->closed_gop = buffer[7] >> 6 & 0x1;
	packet->broken_link = buffer[7]>>5 & 0x1;
}


typedef struct TS_PICTURE_HEADER
{
	unsigned picture_start_code;//32bit;
	unsigned temporal_reference;//10bit;
	unsigned picture_coding_type;//3bit;
	unsigned vbv_delay;//16bit;
	unsigned full_pel_forward_vector;//1bit;
	unsigned forward_f_code;//3bit

} TS_PICTURE_HEADER;


void adjust_picture_header( TS_PICTURE_HEADER * packet, unsigned char * buffer )
{
	packet->picture_start_code  = buffer[0]<<24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
	packet->temporal_reference  = buffer[4] << 2  | (buffer[5] >> 6 & 0x3);
	packet->picture_coding_type = buffer[5] >> 3 & 0x7;
	packet->vbv_delay           = (buffer[5] & 0x7 << 13 )| buffer[6] << 5 | (buffer[7] >>3 & 0x1f);

	if(packet->picture_coding_type == 2 || packet->picture_coding_type == 3)
	{
		packet->full_pel_forward_vector = buffer[7] >>2 & 0x1;
		packet->forward_f_code = (buffer[7] & 0x3) <<1 | (buffer[8] >> 7 & 0x1);
	}
}

typedef struct TS_PICTURE_CODING_EXTENSION
{
	unsigned extension_start_code;//32bit;
	unsigned extension_start_code_indentifier;//4bit
	unsigned f_code[2][2];// 4 * 4bit;
	unsigned intra_dc_precision;//2bit;
	unsigned picture_structure;//2bit;
	unsigned top_field_first;//1bit;
	unsigned frame_pred_frame_dct;//1bit;
	unsigned concealment_motion_vectors;//1bit;
	unsigned q_scale_type; //1bit;
	unsigned intra_vlc_format;//1bit;
	unsigned alternate_scan;//1bit;
	unsigned repeat_first_field;//1bit;
	unsigned chroma_420_type;//1bit;
	unsigned progressive_frame;//1bit;
	unsigned composite_display_flag;//1bit;
	unsigned v_axis;//1bit;
	unsigned field_sequence;//3bit;
	unsigned sub_carrier;//1bit;
	unsigned burst_amplitude;//7bit;
	unsigned sub_carrier_phase;//8bit;
} TS_PICTURE_CODING_EXTENSION;

void adjust_picture_coding_extension( TS_PICTURE_CODING_EXTENSION * packet, unsigned char * buffer )
{
	packet->extension_start_code  = buffer[0]<<24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
	packet->extension_start_code_indentifier  = buffer[4]>>4 & 0xf;
	packet->f_code[0][0] = buffer[4] & 0xf;
	packet->f_code[0][1] = buffer[5]>>4 & 0xf;
	packet->f_code[1][0] = buffer[5] & 0xf;
	packet->f_code[1][1] = buffer[6]>>4 & 0xf;
	packet->intra_dc_precision =buffer[6]>>2 &0x3;
	packet->picture_structure =buffer[6] &0x3;
	packet->top_field_first =buffer[7] >>6 &0x1;
	packet->frame_pred_frame_dct =buffer[7] >>5 &0x1;
	packet->concealment_motion_vectors =buffer[7] >>4 &0x1;
	packet->q_scale_type =buffer[7] >>3 &0x1;
	packet->intra_vlc_format =buffer[7] >>2 &0x1;
	packet->alternate_scan =buffer[7] >>1 &0x1;
	packet->repeat_first_field =buffer[7] &0x1;
	packet->chroma_420_type =buffer[8]>>7 &0x1;
	packet->progressive_frame =buffer[8]>>6 &0x1;
	packet->composite_display_flag =buffer[8]>>5 &0x1;
	if(packet->composite_display_flag)
	{
		packet->v_axis =buffer[8]>>4 &0x1;
		packet->field_sequence =buffer[8]>>1 &0x7;
		packet->sub_carrier = buffer[8] &0x1;
		packet->burst_amplitude = buffer[9]>>1 & 0x7f;
		packet->sub_carrier_phase = (buffer[9] & 0x1)<<7 |buffer[10]>>1 & 0x7f;
	}
}



typedef struct TS_PICTURE_DATA
{
	unsigned slice_start_code;//32bit
	unsigned slice_vertical_position_extension;//3bit
	unsigned priority_breakpoint;//7bit
	unsigned quantiser_scale_code;//5bit
	unsigned intra_slice_flag;//1bit
	unsigned intra_slice;//1bit;
	unsigned reserved_bits;//7bit;
	unsigned extra_bit_slice;//1bit
	unsigned extra_information_slice;//8bit
} TS_PICTURE_DATA;

void adjust_picture_data( TS_PICTURE_DATA * packet, unsigned char * buffer )
{
	packet->slice_start_code  = buffer[0]<<24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
	packet->quantiser_scale_code  = buffer[4]>>3 & 0x1f;

	if( (buffer[4]>>2&0x1) == '1')
	{

	}

	packet->extra_bit_slice = buffer[4]>>2 & 0x1f;

}

void ParserTSHeaders(FILE *pFile,std::vector<TS_packet_header> & listTSHeader)
{


	unsigned char packet_header[4];
	int i = 0;

	//fseek(pFile,0,SEEK_SET);

	while(fread(packet_header,1,4,pFile) > 0)
	{
		fseek(pFile,188*(i++),SEEK_SET);
		TS_packet_header opacket_header;
		adjust_TS_packet_header(&opacket_header,packet_header);

		opacket_header.print();
		listTSHeader.push_back(opacket_header);

		if( i == 10) break;
	}
}

int main(void) {

   FILE *pFile;
	pFile=fopen("/home/hanzhongtao/Videos/big_buck_bunny-1.ts", "rb");
	if(pFile==NULL)
	{
		puts("open file failed!");
		return 0;
	}
	vector<TS_packet_header>  listTSHeader;
	//ParserTSHeaders(pFile,listTSHeader);

	TS_PAT  oPAT;
	TS_PMT  oPMT;
	TS_PES_H oPESHeader;
	TS_SEQUENCE_HEADER oSequnceHeader;
	TS_SEQUENCE_EXTENSION oSequnceExtension;
	TS_GROUP_OF_PICTURES oGroupOfPictures;

	TS_PICTURE_HEADER oPictureHeader;
	TS_PICTURE_CODING_EXTENSION oPictureCodeingExtension;
	TS_PICTURE_DATA oPictureData;
	unsigned char packet_header[4];
	int i = 0;
	fseek(pFile,0,SEEK_SET);
	while(1)
	{
		fseek(pFile,188*i,SEEK_SET);

		if(fread(packet_header,1,4,pFile) <= 0)
		{
			break;
		}

		TS_packet_header opacket_header;
		adjust_TS_packet_header(&opacket_header,packet_header);
		opacket_header.print();
		listTSHeader.push_back(opacket_header);
		TS_AF  oAF;


		int adaption_total_length = 0;
		if(opacket_header.adaption_field_control == 3 || opacket_header.adaption_field_control == 1)
		{

			fseek(pFile,188*i+4,SEEK_SET);

			unsigned char packet_adaptation[20];
			fread(packet_adaptation,1,20,pFile);
			adjust_Adaptation_field(&oAF,packet_adaptation);

			adaption_total_length +=1;//adaptation_fild_length own.
			adaption_total_length +=oAF.adaptation_fild_length;
		}

		if(opacket_header.PID == 0)
		{
			fseek(pFile,188*i+ 4 + adaption_total_length ,SEEK_SET);

			unsigned char packet_pat[20];
			fread(packet_pat,1,20,pFile);
			adjust_PAT_table(&oPAT,packet_pat);
		}

		if(opacket_header.PID  == oPAT.program_map_PID)
		{
			fseek(pFile,188*i+ 4 + adaption_total_length ,SEEK_SET);

			unsigned char packet_pmt[20];
			fread(packet_pmt,1,20,pFile);
			adjust_PMT_table(&oPMT,packet_pmt);
		}

		if(	g_es[0].pid == opacket_header.PID )
		{
			if(opacket_header.payload_unit_start_indicator == 1)
			{
				fseek(pFile,188*i+ 4 + adaption_total_length ,SEEK_SET);
				unsigned char packet_pes_header[20];
				fread(packet_pes_header,1,20,pFile);
				adjust_pes_header(&oPESHeader,packet_pes_header);
				fseek(pFile,188*i+ 4 + adaption_total_length  + 6 + 2 +1 + oPESHeader.PES_header_dada_length,SEEK_SET);//6 B haeder,2B flag,1B length.

				unsigned char packet_sequence_header[20];
				fread(packet_sequence_header,1,20,pFile);
				adjust_sequence_header(&oSequnceHeader, packet_sequence_header);

				fseek(pFile,188*i+ 4 + adaption_total_length  + 6 + 2 +1 + oPESHeader.PES_header_dada_length + 12 ,SEEK_SET);//6 B haeder,2B flag,1B length.
				unsigned char packet_sequence_extension[20];
				fread(packet_sequence_extension,1,20,pFile);
				adjust_sequence_extension(&oSequnceExtension, packet_sequence_extension);


				fseek(pFile,188*i+ 4 + adaption_total_length  + 6 + 2 +1 + oPESHeader.PES_header_dada_length + 12 + 10,SEEK_SET);//6 B haeder,2B flag,1B length.
				unsigned char packet_group_of_pictures[20];
				fread(packet_group_of_pictures,1,20,pFile);
				adjust_group_of_pictures(&oGroupOfPictures, packet_group_of_pictures);

				fseek(pFile,188*i+ 4 + adaption_total_length  + 6 + 2 +1 + oPESHeader.PES_header_dada_length + 12 + 10 + 8,SEEK_SET);//6 B haeder,2B flag,1B length.
				unsigned char packet_picture_header[20];
				fread(packet_picture_header,1,20,pFile);
				adjust_picture_header(&oPictureHeader, packet_picture_header);


				fseek(pFile,188*i+ 4 + adaption_total_length  + 6 + 2 +1 + oPESHeader.PES_header_dada_length + 12 + 10 + 8 + 8,SEEK_SET);//6 B haeder,2B flag,1B length.
				unsigned char packet_picture_coding_extension[20];
				fread(packet_picture_coding_extension,1,20,pFile);
				adjust_picture_coding_extension(&oPictureCodeingExtension, packet_picture_coding_extension);

				fseek(pFile,188*i+ 4 + adaption_total_length  + 6 + 2 +1 + oPESHeader.PES_header_dada_length + 12 + 10 + 8 + 8 + 9,SEEK_SET);//6 B haeder,2B flag,1B length.
				unsigned char packet_picture_data[20];
				fread(packet_picture_data,1,20,pFile);
				adjust_picture_data(&oPictureData, packet_picture_data);


			}

			break;
		}
		i++;
	}

/*
	int nTSIndex = 0;
	unsigned char packet_header[16];
	fseek(pFile,188*nTSIndex,SEEK_SET);
	fread(packet_header,1,15,pFile);
	//packet_header[5] = '\0';

	TS_packet_header opacket_header;
	adjust_TS_packet_header(&opacket_header,packet_header);

	if(opacket_header.adaption_field_control == 3 || opacket_header.adaption_field_control == 1)
	{
		fseek(pFile,188*nTSIndex+4,SEEK_SET);
		TS_AF  oAF;
		unsigned char packet_pat[20];
		fread(packet_pat,1,20,pFile);
		adjust_Adaptation_field(&oAF,packet_pat);
	}

	if(nTSIndex == 1)
	{
		fseek(pFile,188*nTSIndex+5,SEEK_SET);
		TS_PAT  oPAT;
		unsigned char packet_pat[20];
		fread(packet_pat,1,20,pFile);
		adjust_PAT_table(&oPAT,packet_pat);
	}
	unsigned char packet_pes[20];
	TS_PES_H oPESHeader;
	TS_SEQUENCE_HEADER oSequnceHeader;
	if(opacket_header.adaption_field_control == 3 || opacket_header.adaption_field_control == 1)
	{
		fseek(pFile,188*nTSIndex+4,SEEK_SET);
		TS_AF  oAF;
		unsigned char packet_pat[20];
		fread(packet_pat,1,20,pFile);
		adjust_Adaptation_field(&oAF,packet_pat);

		fseek(pFile,188*nTSIndex + 4 + 1+ oAF.adaptation_fild_length,SEEK_SET);

		fread(packet_pes,1,20,pFile);

		adjust_pes_header ( &oPESHeader, packet_pes );

		fseek(pFile,188*nTSIndex + 4 + 1+ oAF.adaptation_fild_length + 6 + 2 +1 + oPESHeader.PES_header_dada_length,SEEK_SET);//6 B haeder,2B flag,1B length.

		unsigned char packet_sequence_header[20];
		fread(packet_sequence_header,1,20,pFile);

		adjust_sequence_header(&oSequnceHeader, packet_sequence_header);

	}

	fseek(pFile,188*nTSIndex+5,SEEK_SET);
	TS_PMT  oPMT;
	unsigned char packet_pmt[20];
	fread(packet_pmt,1,20,pFile);
	adjust_PMT_table(&oPMT,packet_pmt);

*/
	return EXIT_SUCCESS;
}
