#include <stdio.h>
#include "int_coding.h"
#include <root.h>

int encodeMSGHeader(int msg_type, uint16_t transid, uint8_t *buff)
{
	buff[0] = msg_type;
	int16_cpy(buff+1, transid);
 
	return 3;
}

int decodeMSGHeader(int * msg_type, uint16_t * transid, uint8_t *buff)
{
	*msg_type 	= buff[0];
	*transid 	= int16_rcpy(buff+1);
 
	return 3;
}

int encodeFECHeader(FECHeader_t * c, uint8_t *buff)
{
	int_cpy(buff, c->sequence_number);
	int_cpy(buff+4, c->grapes_id);
	int_cpy(buff+8, c->reserved1);
	int_cpy(buff+12, c->crc_checksum);

	return 16;
}

int decodeFECHeader(FECHeader_t * c, const uint8_t *buff)
{
	c->sequence_number = int_rcpy(buff);
	c->grapes_id = int_rcpy(buff+4);
	c->reserved1 = int_rcpy(buff+8);
	c->crc_checksum = int_rcpy(buff+12);

	return 16;
}

int encodePayloadHeader(PayloadHeader_t * c, uint8_t *buff)
{
  	uint32_t half_ts;

	int_cpy(buff, c->codec_type);
	int_cpy(buff+4, c->codec_id);
	buff[8] = c->flags;
	buff[9] = c->marker;

	half_ts = c->timestamp_90KHz >> 32;
	int_cpy(buff + 10, half_ts);
	half_ts = c->timestamp_90KHz;
	int_cpy(buff + 14, half_ts);

	int16_cpy(buff+18, c->meta_data[0]);
 	int16_cpy(buff+20, c->meta_data[1]);

	int_cpy(buff + 22, c->frame_count);

	return 26;
}

int decodePayloadHeader(PayloadHeader_t * c, const uint8_t *buff)
{
	c->codec_type = int_rcpy(buff);
	c->codec_id = int_rcpy(buff+4);
	c->flags = buff[8];
	c->marker = buff[9];
	
	c->timestamp_90KHz = int_rcpy(buff + 10);
	c->timestamp_90KHz = c->timestamp_90KHz << 32;
	c->timestamp_90KHz |= int_rcpy(buff + 14); 

	c->meta_data[0] = int16_rcpy(buff + 18);
	c->meta_data[1] = int16_rcpy(buff + 20);

	c->frame_count= int_rcpy(buff + 22); 

	return 26;
}

