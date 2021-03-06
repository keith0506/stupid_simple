#include "sender.h"
#include "rtc_base/location.h"
#include "rtc_base/timeutils.h"
#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/rtp_headers.h"
#include "rtc_base/socket.h"
#include "api/transport/goog_cc_factory.h"
#include "api/bitrate_constraints.h"
#include <memory.h>
namespace zsy{
const uint32_t kInitialBitrateBps = 500000;
Sender::Sender(){
	bin_stream_init(&stream_);
	clock_=webrtc::Clock::GetRealTimeClock();
	inner_cf_=new webrtc::GoogCcNetworkControllerFactory(&m_nullLog);
	webrtc::BitrateConstraints rate_config;
	rate_config.min_bitrate_bps=kInitialBitrateBps;
	rate_config.start_bitrate_bps=2*kInitialBitrateBps;
	rate_config.max_bitrate_bps=5*kInitialBitrateBps;
	controller_.reset(new webrtc::RtpTransportControllerSend(clock_,
			&m_nullLog,inner_cf_,rate_config));
	pacer_=controller_->packet_sender();
	controller_->RegisterTargetTransferRateObserver(this);
	rtp_rtcp_=new FakeRtpRtcpImpl(this);
	rtp_rtcp_->SetSSRC(uid_);
	webrtc::PacketRouter *router=controller_->packet_router();
	router->AddSendRtpModule(rtp_rtcp_,true);
	controller_->OnNetworkAvailability(true);
	controller_->EnablePeriodicAlrProbing(true);
}
Sender::~Sender(){
	bin_stream_destroy(&stream_);
	webrtc::PacketRouter *router=controller_->packet_router();
	router->RemoveSendRtpModule(rtp_rtcp_);
	delete  rtp_rtcp_;
	{
		rtc::CritScope cs(&buf_mutex_);
		while(!pending_buf_.empty()){
			sim_segment_t *seg=NULL;
			auto it=pending_buf_.begin();
			seg=it->second;
			pending_buf_.erase(it);
			delete seg;
		}
	}
}
void Sender::SetEncoder(VideoSource *encoder){
	encoder_=encoder;
	encoder_->RegisterVideoTarget(this);
}
void Sender::Bind(char*ip,uint16_t port){
	 su_udp_create(ip,port,&fd_);
}
void Sender::SetPeer(char* addr){
	 su_string_to_addr(&dst,addr);
}
void Sender::Start(){
	running_=true;
	if(encoder_){
		encoder_->SetMinRate(kInitialBitrateBps);
		encoder_->Start();		
	}
}
void Sender::Stop(){
	running_=false;
	if(encoder_){
		encoder_->Stop();
	}
}
void Sender::Process(){
	if(!running_){
		return;
	}
	int64_t now=rtc::TimeMillis();
	uint32_t delta=100;
	if(rtt_!=0){
		delta=rtt_;
	}
	if(now-update_ping_ts_>delta){
		SendPing(now);
	}
	su_addr remote;
	bin_stream_t rstream;
	bin_stream_init(&rstream);
	int32_t ret=0;
	ret=su_udp_recv(fd_,&remote,rstream.data,rstream.size,0);
	if(ret>0){
		memcpy(&dst,&remote,sizeof(su_addr));
		rstream.used=ret;
		ProcessingMsg(&rstream);
	}
	bin_stream_destroy(&rstream);
}
#define MAX_SPLIT_NUMBER	1024
static uint16_t FrameSplit(uint16_t splits[], size_t size){
	uint16_t ret, i;
	uint16_t remain_size;

	if (size <= SIM_VIDEO_SIZE){
		ret = 1;
		splits[0] = size;
	}
	else{
		ret = size / SIM_VIDEO_SIZE;
		for (i = 0; i < ret; i++)
			splits[i] = SIM_VIDEO_SIZE;

		remain_size = size % SIM_VIDEO_SIZE;
		if (remain_size > 0){
			splits[ret] = remain_size;
			ret++;
		}
	}

	return ret;
}
void Sender::SendVideo(uint8_t payload_type,int ftype,
		void *data,uint32_t len){
	if(!running_){
		return;
	}
	if(len==0){
		return;
	}
	uint32_t timestamp=0;
	int64_t now=rtc::TimeMillis();
	if(first_ts_==-1){
		first_ts_=now;
	}
	timestamp=now-first_ts_;
	uint8_t* pos;
	uint16_t splits[MAX_SPLIT_NUMBER], total=0;
	assert((len / SIM_VIDEO_SIZE) < MAX_SPLIT_NUMBER);
	total = FrameSplit(splits, len);
	pos = (uint8_t*)data;
	std::map<uint16_t,uint16_t> seq_len_map;
	int i=0;
	{
		rtc::CritScope cs(&buf_mutex_);
		for(i=0;i<total;i++){
			sim_segment_t *seg=new sim_segment_t();
			uint16_t overhead=0;
			seg->packet_id=packet_seed_;
			packet_seed_++;
			seg->fid = frame_seed_;
			seg->timestamp = timestamp;
			seg->ftype = ftype;
			seg->payload_type = payload_type;
			seg->index = i;
			seg->total = total;
			seg->remb = 1;
			seg->send_ts = 0;
			seg->transport_seq =trans_seq_;
			trans_seq_++;
			seg->data_size = splits[i];
			memcpy(seg->data, pos, seg->data_size);
			pos += splits[i];
			overhead=seg->data_size+SIM_SEGMENT_HEADER_SIZE;
			pending_buf_.insert(std::make_pair(seg->transport_seq,seg));
			seq_len_map.insert(std::make_pair(seg->transport_seq,overhead));
		}
	}
	frame_seed_++;
	for(auto it=seq_len_map.begin();it!=seq_len_map.end();it++){
		uint16_t seq=it->first;
		uint16_t overhead=it->second;
		pacer_->InsertPacket(webrtc::PacedSender::kNormalPriority,uid_,
				seq,now,overhead,false);
	}
}
bool Sender::TimeToSendPacket(uint32_t ssrc,
                              uint16_t sequence_number,
                              int64_t capture_time_ms,
                              bool retransmission,
                              const webrtc::PacedPacketInfo& cluster_info){
	bool ret =true;
	sim_segment_t *seg=get_segment_t(sequence_number);
	int64_t now=rtc::TimeMillis();
	if(seg){
		SendSegment(seg,now);
		uint16_t overhead=seg->data_size+SIM_SEGMENT_HEADER_SIZE;
		if(controller_){
			controller_->AddPacket(uid_,sequence_number,overhead,cluster_info);
			rtc::SentPacket sentPacket((int64_t)sequence_number,now);
			controller_->OnSentPacket(sentPacket);
		}
		delete seg;
	}
	return ret;
}
#define MAX_PAD_SIZE 500
int  Sender::SendPadding(uint16_t payload_len,uint32_t ts,const webrtc::PacedPacketInfo& pacing_info){
    sim_header_t header;
	sim_pad_t pad;
	uint32_t header_len=sizeof(sim_header_t)+sizeof(sim_pad_t);
	pad.data_size=payload_len;
	pad.send_ts=ts;
	uint16_t sequence_number=trans_seq_;
	pad.transport_seq=sequence_number;
	trans_seq_++;
	int overhead=header_len+payload_len;
	INIT_SIM_HEADER(header, SIM_PAD, uid_);
	sim_encode_msg(&stream_, &header, &pad);
	SendToNetwork(stream_.data,stream_.used);
	if(controller_){
		controller_->AddPacket(uid_,sequence_number,overhead,pacing_info);
		rtc::SentPacket sentPacket((int64_t)sequence_number,ts);
    	controller_->OnSentPacket(sentPacket);
    }
	return overhead;
}
size_t Sender::TimeToSendPadding(size_t bytes,
                                 const webrtc::PacedPacketInfo& cluster_info){
	int64_t now=rtc::TimeMillis();
	int remain=bytes;
	if(bytes<100){

	}else{
		while(remain>0)
		{
			if(remain>=MAX_PAD_SIZE){
				int ret=SendPadding(MAX_PAD_SIZE,now,cluster_info);
				remain-=ret;
			}
			else{
				if(remain>=100){
					SendPadding(remain,now,cluster_info);
				}
				if(remain>0){
					remain=0;
				}
			}
		}
	}
	return bytes;
}
void Sender::RTT(int64_t* rtt,
         int64_t* avg_rtt,
         int64_t* min_rtt,
         int64_t* max_rtt){
	*rtt=rtt_;
	*avg_rtt=average_rtt_;
	*min_rtt=min_rtt_;
	*max_rtt=max_rtt_;
}
void Sender::OnTargetTransferRate(webrtc::TargetTransferRate rate){
	int64_t bps=rate.target_rate.bps();
	std::cout<<"rate "<<bps<<std::endl;
	if(encoder_){
		encoder_->ChangeRate(bps);
	}
}
void Sender::OnStartRateUpdate(webrtc::DataRate rate){
	int64_t bps=rate.bps();
	std::cout<<"rate "<<bps<<std::endl;
	if(encoder_){
		encoder_->ChangeRate(bps);
	}
}
sim_segment_t *Sender::get_segment_t(uint16_t sequence_number){
	sim_segment_t *seg=NULL;
	rtc::CritScope cs(&buf_mutex_);
	auto it=pending_buf_.find(sequence_number);
	if(it!=pending_buf_.end()){
		seg=it->second;
		pending_buf_.erase(it);
	}
	return seg;
}
void Sender::SendToNetwork(uint8_t*data,uint32_t len){
	su_udp_send(fd_,&dst,data, len);
}
void Sender::ProcessingMsg(bin_stream_t *stream){
	sim_header_t header;
	if (sim_decode_header(stream, &header) != 0)
		return;
	if (header.mid < MIN_MSG_ID || header.mid > MAX_MSG_ID)
		return;
	switch(header.mid){
	case SIM_PING:{
		sim_header_t h;
		sim_ping_t ping;
		sim_decode_msg(stream, &header, &ping);
		INIT_SIM_HEADER(h, SIM_PONG, uid_);
		sim_encode_msg(stream, &h, &ping);
		SendToNetwork(stream->data,stream->used);
        break;
	}
	case SIM_PONG:{
		sim_pong_t pong;
		sim_decode_msg(stream, &header, &pong);
		int64_t now=rtc::TimeMillis();
		int64_t rtt=now-pong.ts;
		if(rtt>0){
			UpdateRtt(rtt,now);
		}
        break;
	}
	case SIM_FEEDBACK:{
		sim_feedback_t feedback;
		if (sim_decode_msg(stream, &header, &feedback) != 0)
			return;
			InnerProcessFeedback(&feedback);
			break;
	}
	default:{
		break;
	}
	}
}
void Sender::SendPing(int64_t now){
	sim_header_t header;
	sim_ping_t body;
	INIT_SIM_HEADER(header, SIM_PING, uid_);
	body.ts = now;
	sim_encode_msg(&stream_, &header, &body);
	SendToNetwork(stream_.data,stream_.used);
	update_ping_ts_=now;
}
void Sender::UpdateRtt(uint32_t time,int64_t now){
	uint32_t keep_rtt=5;
	if(time>keep_rtt){
		keep_rtt=time;
	}
	if(rtt_==0){
		rtt_=keep_rtt;
	}

	rtt_ = (7 * rtt_ + keep_rtt) / 8;
	if (rtt_ < 10)
		rtt_ = 10;
	if(rtt_num_==0)
	{
		sum_rtt_=keep_rtt;
		max_rtt_=keep_rtt;
		min_rtt_=keep_rtt;
		average_rtt_=keep_rtt;
		rtt_num_++;
		if(controller_){
			controller_->OnRttUpdate(average_rtt_,max_rtt_);
		}
		return;
	}
	rtt_num_+=1;
	sum_rtt_+=keep_rtt;
	average_rtt_=sum_rtt_/rtt_num_;
	if(controller_){
		controller_->OnRttUpdate(average_rtt_,max_rtt_);
	}
	if(keep_rtt>max_rtt_)
	{
		max_rtt_=keep_rtt;
	}
	if(keep_rtt<min_rtt_){
		min_rtt_=keep_rtt;
	}
}
void Sender::InnerProcessFeedback(sim_feedback_t* feedback){
	std::unique_ptr<webrtc::rtcp::TransportFeedback> fb=
	webrtc::rtcp::TransportFeedback::ParseFrom((uint8_t*)feedback->feedback, feedback->feedback_size);
	if(controller_){
        //printf("fb %d\n",feedback->feedback_size);
		controller_->OnTransportFeedback(*fb.get());
	}
}
void Sender::SendSegment(sim_segment_t *seg,uint32_t now){
	sim_header_t header;
	seg->send_ts = (uint16_t)(now - first_ts_- seg->timestamp);
	INIT_SIM_HEADER(header, SIM_SEG, uid_);
	sim_encode_msg(&stream_, &header, seg);
	SendToNetwork(stream_.data,stream_.used);
}
}
