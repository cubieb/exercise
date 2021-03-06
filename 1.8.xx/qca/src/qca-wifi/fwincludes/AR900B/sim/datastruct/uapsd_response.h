// Copyright (c) 2012 Qualcomm Atheros, Inc.  All rights reserved.
// $ATH_LICENSE_HW_HDR_C$
//
// DO NOT EDIT!  This file is automatically generated
//               These definitions are tied to a particular hardware layout


#ifndef _UAPSD_RESPONSE_H_
#define _UAPSD_RESPONSE_H_
#if !defined(__ASSEMBLER__)
#endif

// ################ START SUMMARY #################
//
//	Dword	Fields
//	0	valid_uapsd_queue_bitmap[9:0], qosnull_resp_required[10], uapsd_resp_frame_cnt[14:11], rr_qid_selection[15], uapsd_trigger_tid_map[31:16]
//	1	fes_control_mode[1:0], qid_selection_control[3:2], reserved_1[31:4]
//	2	resp_mpdu_queue0[13:0], queue0_ac[15:14], resp_mpdu_queue1[29:16], queue1_ac[31:30]
//	3	resp_mpdu_queue2[13:0], queue2_ac[15:14], resp_mpdu_queue3[29:16], queue3_ac[31:30]
//	4	resp_mpdu_queue4[13:0], queue4_ac[15:14], resp_mpdu_queue5[29:16], queue5_ac[31:30]
//	5	resp_mpdu_queue6[13:0], queue6_ac[15:14], resp_mpdu_queue7[29:16], queue7_ac[31:30]
//	6	resp_mpdu_queue8[13:0], queue8_ac[15:14], resp_mpdu_queue9[29:16], queue9_ac[31:30]
//	7	scheduler_tlv_ptr[31:0]
//	8	next_q_bitmap_id_to_evaluate[3:0], reserved_8[31:4]
//
// ################ END SUMMARY #################

#define NUM_OF_DWORDS_UAPSD_RESPONSE 9

struct uapsd_response {
    volatile uint32_t valid_uapsd_queue_bitmap        : 10, //[9:0]
                      qosnull_resp_required           :  1, //[10]
                      uapsd_resp_frame_cnt            :  4, //[14:11]
                      rr_qid_selection                :  1, //[15]
                      uapsd_trigger_tid_map           : 16; //[31:16]
    volatile uint32_t fes_control_mode                :  2, //[1:0]
                      qid_selection_control           :  2, //[3:2]
                      reserved_1                      : 28; //[31:4]
    volatile uint32_t resp_mpdu_queue0                : 14, //[13:0]
                      queue0_ac                       :  2, //[15:14]
                      resp_mpdu_queue1                : 14, //[29:16]
                      queue1_ac                       :  2; //[31:30]
    volatile uint32_t resp_mpdu_queue2                : 14, //[13:0]
                      queue2_ac                       :  2, //[15:14]
                      resp_mpdu_queue3                : 14, //[29:16]
                      queue3_ac                       :  2; //[31:30]
    volatile uint32_t resp_mpdu_queue4                : 14, //[13:0]
                      queue4_ac                       :  2, //[15:14]
                      resp_mpdu_queue5                : 14, //[29:16]
                      queue5_ac                       :  2; //[31:30]
    volatile uint32_t resp_mpdu_queue6                : 14, //[13:0]
                      queue6_ac                       :  2, //[15:14]
                      resp_mpdu_queue7                : 14, //[29:16]
                      queue7_ac                       :  2; //[31:30]
    volatile uint32_t resp_mpdu_queue8                : 14, //[13:0]
                      queue8_ac                       :  2, //[15:14]
                      resp_mpdu_queue9                : 14, //[29:16]
                      queue9_ac                       :  2; //[31:30]
    volatile uint32_t scheduler_tlv_ptr               : 32; //[31:0]
    volatile uint32_t next_q_bitmap_id_to_evaluate    :  4, //[3:0]
                      reserved_8                      : 28; //[31:4]
};

/*

valid_uapsd_queue_bitmap
			
			Valid response queue bitmap in case a valid U-APSD
			trigger frame was received.
			
			
			
			The bitmap indicates which of the following QIDs are
			valid and from which U-APSD response data frames are allowed
			to be fetched from this queue.
			
			
			
			The scheduler will evaluate the valid MPDU queues one at
			a time (starting from Resp_mpdu_queue0) and stop when the
			first queue with data is found.
			
			The response data will be sent from this queue.
			
			
			
			When all queues are empty, no data will be sent back.
			Depending on the setting of QoSNull_resp_required, only a
			QoS Null frame will be sent instead.
			
			 <legal all>

qosnull_resp_required
			
			Field only valid in case of U-APSD response handling
			
			
			
			0: No action
			
			1: Transmit a QoS Null frame when all enabled response
			mpdu queues are empty. 
			
			The QID used for the transmit command will be the first
			valid QID.
			
			
			
			<legal all>

uapsd_resp_frame_cnt
			
			Field only valid in case of U-APSD response handling
			
			
			
			When a valid uapsd trigger is received, and there is
			data available in one of the trigger enabled transmit
			queues, transmit this maximum number of frames.
			
			A value of 7 indicates, to drain the complete transmit
			queue.
			
			If the number of response frames is not met (due to
			shortage of TXOP time, not enough frames in this MPDU queue,
			but other queues still having some,  this will be reported
			in the FES STATUS TLV. SW will have to schedule the next
			transmit command.
			
			<legal all>

rr_qid_selection
			
			Round Robin QueueID selection
			
			When set, the Scheduler will transmit from queues in a
			Round Robin method, always remembering that after
			transmission from a queue, the scheduler should try and
			transmit from the next queue first.
			
			For this it updates the Next_q_bitmap_id_to_evaluate
			field after each successful transmission.
			
			<legal all>

uapsd_trigger_tid_map
			
			Field used to determine if the incoming QoS data frame
			is a U-APSD trigger frame.
			
			
			
			Bit 16 corresponds to TID 0 
			
			Bit 17 to TiD1 etc.
			
			
			
			Values
			
			0: The received QoS data frame with the corresponding
			TID value is NOT an uapsd trigger frame
			
			1: The received QoS data frame with the corresponding
			TID value IS an uapsd trigger frame. Response data from
			trigger enabled transmit queues (indicated in the
			valid_uapsd_queue_bitmap) should be sent.
			
			<legal all>

fes_control_mode
			
			1-0: Reserved
			
			2: SW initiated transmission (initiated by SIFS response
			table) For this setting the scheduler will NOT perform any
			checks with the QM. SW has preprogrammed the entire response
			(data frames or a NULL frame) in the scheduling command.
			
			3: HW initiated transmission (initiated by SIFS response
			table) For this setting the scheduler will perform all the
			MPDU queue checks with the QM. Based on the result of those
			checks and other settings in this descriptor, the scheduler
			might initiate a data transmission, null transmission or not
			initiate a transmission at all.
			
			<legal 3-2>

qid_selection_control
			
			<enum 0 Ordered> Scheduler will search for an MPDU queue
			with data starting from Resp_mpdu_queue0, going to
			Resp_mpdu_queue9
			
			<enum 1 RR> Round Robin QueueID selection
			
			When set, the Scheduler will transmit from queues in a
			Round Robin method, always remembering that after
			transmission from a queue, the scheduler should try and
			transmit from the next queue first.
			
			For this it updates the Next_q_bitmap_id_to_evaluate
			field after each successful transmission.
			
			<enum 2 Same_AC>  Only transmit from the queues with the
			same AC as the incoming trigger frame (AC derived from the
			TID)
			
			<enum 3 Same_AC or lower >  Only transmit from the
			queues with the same AC  or lower as the incoming trigger
			frame (AC derived from the TID)

reserved_1
			
			<legal 0>

resp_mpdu_queue0
			
			Field only valid when valid_ uapsd_queue_bitmap[0] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>

queue0_ac
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  

resp_mpdu_queue1
			
			Field only valid when valid_ uapsd_queue_bitmap[1] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>

queue1_ac
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  

resp_mpdu_queue2
			
			Field only valid when valid_ uapsd_queue_bitmap[2] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>

queue2_ac
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  

resp_mpdu_queue3
			
			Field only valid when valid_ uapsd_queue_bitmap[3] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>

queue3_ac
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  

resp_mpdu_queue4
			
			Field only valid when valid_ uapsd_queue_bitmap[4] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>

queue4_ac
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  

resp_mpdu_queue5
			
			Field only valid when valid_ uapsd_queue_bitmap[5] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>

queue5_ac
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  

resp_mpdu_queue6
			
			Field only valid when valid_ uapsd_queue_bitmap[6] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>

queue6_ac
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  

resp_mpdu_queue7
			
			Field only valid when valid_ uapsd_queue_bitmap[7] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>

queue7_ac
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  

resp_mpdu_queue8
			
			Field only valid when valid_ uapsd_queue_bitmap[8] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>

queue8_ac
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  

resp_mpdu_queue9
			
			Field only valid when valid_ uapsd_queue_bitmap[9] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			<legal all>

queue9_ac
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  

scheduler_tlv_ptr
			
			The address location for the transmit TLVs.
			
			These TLVs get read/interpreted by the scheduler and
			when applicable pushed down the transmit chain to the other
			transmit modules. 
			
			<legal all>

next_q_bitmap_id_to_evaluate
			
			Only valid when RR_qid_selection is set.
			
			
			
			This field indicates the next queue bitmap id from which
			the scheduler should try to transmit first on reception of a
			new PS-POLL frame.
			
			
			
			This field is put in a separate word, as HW will update
			this field on regular basis.
			
			<legal 9-0>

reserved_8
			
			<legal 0>
*/


/* Description		UAPSD_RESPONSE_0_VALID_UAPSD_QUEUE_BITMAP
			
			Valid response queue bitmap in case a valid U-APSD
			trigger frame was received.
			
			
			
			The bitmap indicates which of the following QIDs are
			valid and from which U-APSD response data frames are allowed
			to be fetched from this queue.
			
			
			
			The scheduler will evaluate the valid MPDU queues one at
			a time (starting from Resp_mpdu_queue0) and stop when the
			first queue with data is found.
			
			The response data will be sent from this queue.
			
			
			
			When all queues are empty, no data will be sent back.
			Depending on the setting of QoSNull_resp_required, only a
			QoS Null frame will be sent instead.
			
			 <legal all>
*/
#define UAPSD_RESPONSE_0_VALID_UAPSD_QUEUE_BITMAP_OFFSET             0x00000000
#define UAPSD_RESPONSE_0_VALID_UAPSD_QUEUE_BITMAP_LSB                0
#define UAPSD_RESPONSE_0_VALID_UAPSD_QUEUE_BITMAP_MASK               0x000003ff

/* Description		UAPSD_RESPONSE_0_QOSNULL_RESP_REQUIRED
			
			Field only valid in case of U-APSD response handling
			
			
			
			0: No action
			
			1: Transmit a QoS Null frame when all enabled response
			mpdu queues are empty. 
			
			The QID used for the transmit command will be the first
			valid QID.
			
			
			
			<legal all>
*/
#define UAPSD_RESPONSE_0_QOSNULL_RESP_REQUIRED_OFFSET                0x00000000
#define UAPSD_RESPONSE_0_QOSNULL_RESP_REQUIRED_LSB                   10
#define UAPSD_RESPONSE_0_QOSNULL_RESP_REQUIRED_MASK                  0x00000400

/* Description		UAPSD_RESPONSE_0_UAPSD_RESP_FRAME_CNT
			
			Field only valid in case of U-APSD response handling
			
			
			
			When a valid uapsd trigger is received, and there is
			data available in one of the trigger enabled transmit
			queues, transmit this maximum number of frames.
			
			A value of 7 indicates, to drain the complete transmit
			queue.
			
			If the number of response frames is not met (due to
			shortage of TXOP time, not enough frames in this MPDU queue,
			but other queues still having some,  this will be reported
			in the FES STATUS TLV. SW will have to schedule the next
			transmit command.
			
			<legal all>
*/
#define UAPSD_RESPONSE_0_UAPSD_RESP_FRAME_CNT_OFFSET                 0x00000000
#define UAPSD_RESPONSE_0_UAPSD_RESP_FRAME_CNT_LSB                    11
#define UAPSD_RESPONSE_0_UAPSD_RESP_FRAME_CNT_MASK                   0x00007800

/* Description		UAPSD_RESPONSE_0_RR_QID_SELECTION
			
			Round Robin QueueID selection
			
			When set, the Scheduler will transmit from queues in a
			Round Robin method, always remembering that after
			transmission from a queue, the scheduler should try and
			transmit from the next queue first.
			
			For this it updates the Next_q_bitmap_id_to_evaluate
			field after each successful transmission.
			
			<legal all>
*/
#define UAPSD_RESPONSE_0_RR_QID_SELECTION_OFFSET                     0x00000000
#define UAPSD_RESPONSE_0_RR_QID_SELECTION_LSB                        15
#define UAPSD_RESPONSE_0_RR_QID_SELECTION_MASK                       0x00008000

/* Description		UAPSD_RESPONSE_0_UAPSD_TRIGGER_TID_MAP
			
			Field used to determine if the incoming QoS data frame
			is a U-APSD trigger frame.
			
			
			
			Bit 16 corresponds to TID 0 
			
			Bit 17 to TiD1 etc.
			
			
			
			Values
			
			0: The received QoS data frame with the corresponding
			TID value is NOT an uapsd trigger frame
			
			1: The received QoS data frame with the corresponding
			TID value IS an uapsd trigger frame. Response data from
			trigger enabled transmit queues (indicated in the
			valid_uapsd_queue_bitmap) should be sent.
			
			<legal all>
*/
#define UAPSD_RESPONSE_0_UAPSD_TRIGGER_TID_MAP_OFFSET                0x00000000
#define UAPSD_RESPONSE_0_UAPSD_TRIGGER_TID_MAP_LSB                   16
#define UAPSD_RESPONSE_0_UAPSD_TRIGGER_TID_MAP_MASK                  0xffff0000

/* Description		UAPSD_RESPONSE_1_FES_CONTROL_MODE
			
			1-0: Reserved
			
			2: SW initiated transmission (initiated by SIFS response
			table) For this setting the scheduler will NOT perform any
			checks with the QM. SW has preprogrammed the entire response
			(data frames or a NULL frame) in the scheduling command.
			
			3: HW initiated transmission (initiated by SIFS response
			table) For this setting the scheduler will perform all the
			MPDU queue checks with the QM. Based on the result of those
			checks and other settings in this descriptor, the scheduler
			might initiate a data transmission, null transmission or not
			initiate a transmission at all.
			
			<legal 3-2>
*/
#define UAPSD_RESPONSE_1_FES_CONTROL_MODE_OFFSET                     0x00000004
#define UAPSD_RESPONSE_1_FES_CONTROL_MODE_LSB                        0
#define UAPSD_RESPONSE_1_FES_CONTROL_MODE_MASK                       0x00000003

/* Description		UAPSD_RESPONSE_1_QID_SELECTION_CONTROL
			
			<enum 0 Ordered> Scheduler will search for an MPDU queue
			with data starting from Resp_mpdu_queue0, going to
			Resp_mpdu_queue9
			
			<enum 1 RR> Round Robin QueueID selection
			
			When set, the Scheduler will transmit from queues in a
			Round Robin method, always remembering that after
			transmission from a queue, the scheduler should try and
			transmit from the next queue first.
			
			For this it updates the Next_q_bitmap_id_to_evaluate
			field after each successful transmission.
			
			<enum 2 Same_AC>  Only transmit from the queues with the
			same AC as the incoming trigger frame (AC derived from the
			TID)
			
			<enum 3 Same_AC or lower >  Only transmit from the
			queues with the same AC  or lower as the incoming trigger
			frame (AC derived from the TID)
*/
#define UAPSD_RESPONSE_1_QID_SELECTION_CONTROL_OFFSET                0x00000004
#define UAPSD_RESPONSE_1_QID_SELECTION_CONTROL_LSB                   2
#define UAPSD_RESPONSE_1_QID_SELECTION_CONTROL_MASK                  0x0000000c

/* Description		UAPSD_RESPONSE_1_RESERVED_1
			
			<legal 0>
*/
#define UAPSD_RESPONSE_1_RESERVED_1_OFFSET                           0x00000004
#define UAPSD_RESPONSE_1_RESERVED_1_LSB                              4
#define UAPSD_RESPONSE_1_RESERVED_1_MASK                             0xfffffff0

/* Description		UAPSD_RESPONSE_2_RESP_MPDU_QUEUE0
			
			Field only valid when valid_ uapsd_queue_bitmap[0] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>
*/
#define UAPSD_RESPONSE_2_RESP_MPDU_QUEUE0_OFFSET                     0x00000008
#define UAPSD_RESPONSE_2_RESP_MPDU_QUEUE0_LSB                        0
#define UAPSD_RESPONSE_2_RESP_MPDU_QUEUE0_MASK                       0x00003fff

/* Description		UAPSD_RESPONSE_2_QUEUE0_AC
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  
*/
#define UAPSD_RESPONSE_2_QUEUE0_AC_OFFSET                            0x00000008
#define UAPSD_RESPONSE_2_QUEUE0_AC_LSB                               14
#define UAPSD_RESPONSE_2_QUEUE0_AC_MASK                              0x0000c000

/* Description		UAPSD_RESPONSE_2_RESP_MPDU_QUEUE1
			
			Field only valid when valid_ uapsd_queue_bitmap[1] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>
*/
#define UAPSD_RESPONSE_2_RESP_MPDU_QUEUE1_OFFSET                     0x00000008
#define UAPSD_RESPONSE_2_RESP_MPDU_QUEUE1_LSB                        16
#define UAPSD_RESPONSE_2_RESP_MPDU_QUEUE1_MASK                       0x3fff0000

/* Description		UAPSD_RESPONSE_2_QUEUE1_AC
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  
*/
#define UAPSD_RESPONSE_2_QUEUE1_AC_OFFSET                            0x00000008
#define UAPSD_RESPONSE_2_QUEUE1_AC_LSB                               30
#define UAPSD_RESPONSE_2_QUEUE1_AC_MASK                              0xc0000000

/* Description		UAPSD_RESPONSE_3_RESP_MPDU_QUEUE2
			
			Field only valid when valid_ uapsd_queue_bitmap[2] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>
*/
#define UAPSD_RESPONSE_3_RESP_MPDU_QUEUE2_OFFSET                     0x0000000c
#define UAPSD_RESPONSE_3_RESP_MPDU_QUEUE2_LSB                        0
#define UAPSD_RESPONSE_3_RESP_MPDU_QUEUE2_MASK                       0x00003fff

/* Description		UAPSD_RESPONSE_3_QUEUE2_AC
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  
*/
#define UAPSD_RESPONSE_3_QUEUE2_AC_OFFSET                            0x0000000c
#define UAPSD_RESPONSE_3_QUEUE2_AC_LSB                               14
#define UAPSD_RESPONSE_3_QUEUE2_AC_MASK                              0x0000c000

/* Description		UAPSD_RESPONSE_3_RESP_MPDU_QUEUE3
			
			Field only valid when valid_ uapsd_queue_bitmap[3] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>
*/
#define UAPSD_RESPONSE_3_RESP_MPDU_QUEUE3_OFFSET                     0x0000000c
#define UAPSD_RESPONSE_3_RESP_MPDU_QUEUE3_LSB                        16
#define UAPSD_RESPONSE_3_RESP_MPDU_QUEUE3_MASK                       0x3fff0000

/* Description		UAPSD_RESPONSE_3_QUEUE3_AC
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  
*/
#define UAPSD_RESPONSE_3_QUEUE3_AC_OFFSET                            0x0000000c
#define UAPSD_RESPONSE_3_QUEUE3_AC_LSB                               30
#define UAPSD_RESPONSE_3_QUEUE3_AC_MASK                              0xc0000000

/* Description		UAPSD_RESPONSE_4_RESP_MPDU_QUEUE4
			
			Field only valid when valid_ uapsd_queue_bitmap[4] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>
*/
#define UAPSD_RESPONSE_4_RESP_MPDU_QUEUE4_OFFSET                     0x00000010
#define UAPSD_RESPONSE_4_RESP_MPDU_QUEUE4_LSB                        0
#define UAPSD_RESPONSE_4_RESP_MPDU_QUEUE4_MASK                       0x00003fff

/* Description		UAPSD_RESPONSE_4_QUEUE4_AC
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  
*/
#define UAPSD_RESPONSE_4_QUEUE4_AC_OFFSET                            0x00000010
#define UAPSD_RESPONSE_4_QUEUE4_AC_LSB                               14
#define UAPSD_RESPONSE_4_QUEUE4_AC_MASK                              0x0000c000

/* Description		UAPSD_RESPONSE_4_RESP_MPDU_QUEUE5
			
			Field only valid when valid_ uapsd_queue_bitmap[5] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>
*/
#define UAPSD_RESPONSE_4_RESP_MPDU_QUEUE5_OFFSET                     0x00000010
#define UAPSD_RESPONSE_4_RESP_MPDU_QUEUE5_LSB                        16
#define UAPSD_RESPONSE_4_RESP_MPDU_QUEUE5_MASK                       0x3fff0000

/* Description		UAPSD_RESPONSE_4_QUEUE5_AC
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  
*/
#define UAPSD_RESPONSE_4_QUEUE5_AC_OFFSET                            0x00000010
#define UAPSD_RESPONSE_4_QUEUE5_AC_LSB                               30
#define UAPSD_RESPONSE_4_QUEUE5_AC_MASK                              0xc0000000

/* Description		UAPSD_RESPONSE_5_RESP_MPDU_QUEUE6
			
			Field only valid when valid_ uapsd_queue_bitmap[6] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>
*/
#define UAPSD_RESPONSE_5_RESP_MPDU_QUEUE6_OFFSET                     0x00000014
#define UAPSD_RESPONSE_5_RESP_MPDU_QUEUE6_LSB                        0
#define UAPSD_RESPONSE_5_RESP_MPDU_QUEUE6_MASK                       0x00003fff

/* Description		UAPSD_RESPONSE_5_QUEUE6_AC
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  
*/
#define UAPSD_RESPONSE_5_QUEUE6_AC_OFFSET                            0x00000014
#define UAPSD_RESPONSE_5_QUEUE6_AC_LSB                               14
#define UAPSD_RESPONSE_5_QUEUE6_AC_MASK                              0x0000c000

/* Description		UAPSD_RESPONSE_5_RESP_MPDU_QUEUE7
			
			Field only valid when valid_ uapsd_queue_bitmap[7] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>
*/
#define UAPSD_RESPONSE_5_RESP_MPDU_QUEUE7_OFFSET                     0x00000014
#define UAPSD_RESPONSE_5_RESP_MPDU_QUEUE7_LSB                        16
#define UAPSD_RESPONSE_5_RESP_MPDU_QUEUE7_MASK                       0x3fff0000

/* Description		UAPSD_RESPONSE_5_QUEUE7_AC
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  
*/
#define UAPSD_RESPONSE_5_QUEUE7_AC_OFFSET                            0x00000014
#define UAPSD_RESPONSE_5_QUEUE7_AC_LSB                               30
#define UAPSD_RESPONSE_5_QUEUE7_AC_MASK                              0xc0000000

/* Description		UAPSD_RESPONSE_6_RESP_MPDU_QUEUE8
			
			Field only valid when valid_ uapsd_queue_bitmap[8] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			 <legal all>
*/
#define UAPSD_RESPONSE_6_RESP_MPDU_QUEUE8_OFFSET                     0x00000018
#define UAPSD_RESPONSE_6_RESP_MPDU_QUEUE8_LSB                        0
#define UAPSD_RESPONSE_6_RESP_MPDU_QUEUE8_MASK                       0x00003fff

/* Description		UAPSD_RESPONSE_6_QUEUE8_AC
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  
*/
#define UAPSD_RESPONSE_6_QUEUE8_AC_OFFSET                            0x00000018
#define UAPSD_RESPONSE_6_QUEUE8_AC_LSB                               14
#define UAPSD_RESPONSE_6_QUEUE8_AC_MASK                              0x0000c000

/* Description		UAPSD_RESPONSE_6_RESP_MPDU_QUEUE9
			
			Field only valid when valid_ uapsd_queue_bitmap[9] is
			set.
			
			
			
			The QID of an MPDU queue from which UAPSD response data
			can be sent.
			
			<legal all>
*/
#define UAPSD_RESPONSE_6_RESP_MPDU_QUEUE9_OFFSET                     0x00000018
#define UAPSD_RESPONSE_6_RESP_MPDU_QUEUE9_LSB                        16
#define UAPSD_RESPONSE_6_RESP_MPDU_QUEUE9_MASK                       0x3fff0000

/* Description		UAPSD_RESPONSE_6_QUEUE9_AC
			
			AC type of this category
			
			<enum 0 Best_Effort> 
			
			<enum 1 Best_Case>  
			
			<enum 2 Video>  
			
			<enum 3 Voice>  
*/
#define UAPSD_RESPONSE_6_QUEUE9_AC_OFFSET                            0x00000018
#define UAPSD_RESPONSE_6_QUEUE9_AC_LSB                               30
#define UAPSD_RESPONSE_6_QUEUE9_AC_MASK                              0xc0000000

/* Description		UAPSD_RESPONSE_7_SCHEDULER_TLV_PTR
			
			The address location for the transmit TLVs.
			
			These TLVs get read/interpreted by the scheduler and
			when applicable pushed down the transmit chain to the other
			transmit modules. 
			
			<legal all>
*/
#define UAPSD_RESPONSE_7_SCHEDULER_TLV_PTR_OFFSET                    0x0000001c
#define UAPSD_RESPONSE_7_SCHEDULER_TLV_PTR_LSB                       0
#define UAPSD_RESPONSE_7_SCHEDULER_TLV_PTR_MASK                      0xffffffff

/* Description		UAPSD_RESPONSE_8_NEXT_Q_BITMAP_ID_TO_EVALUATE
			
			Only valid when RR_qid_selection is set.
			
			
			
			This field indicates the next queue bitmap id from which
			the scheduler should try to transmit first on reception of a
			new PS-POLL frame.
			
			
			
			This field is put in a separate word, as HW will update
			this field on regular basis.
			
			<legal 9-0>
*/
#define UAPSD_RESPONSE_8_NEXT_Q_BITMAP_ID_TO_EVALUATE_OFFSET         0x00000020
#define UAPSD_RESPONSE_8_NEXT_Q_BITMAP_ID_TO_EVALUATE_LSB            0
#define UAPSD_RESPONSE_8_NEXT_Q_BITMAP_ID_TO_EVALUATE_MASK           0x0000000f

/* Description		UAPSD_RESPONSE_8_RESERVED_8
			
			<legal 0>
*/
#define UAPSD_RESPONSE_8_RESERVED_8_OFFSET                           0x00000020
#define UAPSD_RESPONSE_8_RESERVED_8_LSB                              4
#define UAPSD_RESPONSE_8_RESERVED_8_MASK                             0xfffffff0


#endif // _UAPSD_RESPONSE_H_
