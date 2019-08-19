#ifndef PAYLOAD_HEADERS_H
#define PAYLOAD_HEADERS_H

#include <grapes_msg_types.h>

typedef struct FECHeader
{
	unsigned int		sequence_number;
	unsigned int		grapes_id;
	unsigned int		reserved1;
	unsigned int		crc_checksum;
}FECHeader_t, *pFECHeader_t;

typedef struct PayloadHeader
{
	int				codec_type;
	int				codec_id;
	char				flags;
	char				marker;
	int64_t			timestamp_90KHz;	
	unsigned short	meta_data[2];
	unsigned int		frame_count;
}PayloadHeader_t, *pPayloadHeader_t;

#define MSG_HEADER_SIZE				3
#define GRAPES_CHUNK_HEADER_SIZE 		20
#define FEC_HEADER_SIZE				sizeof(FECHeader_t)
#define PAYLOAD_HEADER_SIZE			sizeof(PayloadHeader_t)

#define AVMEDIA_TYPE_VIDEO_1			8
#define AVMEDIA_TYPE_VIDEO_2			9
#define AVMEDIA_TYPE_VIDEO_3			10

#define AVMEDIA_TYPE_VIDEO_US				12

int encodePayloadHeader(PayloadHeader_t * c, uint8_t *buff);
int decodePayloadHeader(PayloadHeader_t * c, const uint8_t *buff);
int encodeFECHeader(FECHeader_t * c, uint8_t *buff);
int decodeFECHeader(FECHeader_t * c, const uint8_t *buff);

#endif

