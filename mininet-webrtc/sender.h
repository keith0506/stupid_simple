#ifndef SENDER_H_
#define SENDER_H_
#include <stdlib.h>
#include "rtc_base/criticalsection.h"
#include "system_wrappers/include/clock.h"
#include "modules/utility/include/process_thread.h"
#include "system_wrappers/include/clock.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/pacing/paced_sender.h"
#include "api/transport/network_control.h"
#include "call/rtp_transport_controller_send.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "sendinterface.h"
#include "fake_rtp_rtcp_impl.h"
#include "videosource.h"
#include "sim_proto.h"
#include "cf_stream.h"
#include "cf_platform.h"
#include <memory>
#include <map>
#include <string>
namespace zsy{
class Sender:public VideoFrameTarget,
public SendInterface,
public webrtc::TargetTransferRateObserver{
public:
	Sender();
	~Sender();
	void SetEncoder(VideoSource *encoder);
	void Bind(char *ip,uint16_t port);
	//"ip:port"
	void SetPeer(char* addr);
	void Start();
	void Stop();
	void Process();
	void SendVideo(uint8_t payload_type,int ftype,void *data,uint32_t len) override;
    virtual bool TimeToSendPacket(uint32_t ssrc,
                                  uint16_t sequence_number,
                                  int64_t capture_time_ms,
                                  bool retransmission,
                                  const webrtc::PacedPacketInfo& cluster_info);
    virtual size_t TimeToSendPadding(size_t bytes,
                                     const webrtc::PacedPacketInfo& cluster_info);
    void RTT(int64_t* rtt,
             int64_t* avg_rtt,
             int64_t* min_rtt,
             int64_t* max_rtt) override;
    virtual void OnTargetTransferRate(webrtc::TargetTransferRate rate) override;
    virtual void OnStartRateUpdate(webrtc::DataRate rate) override;
private:
	sim_segment_t* get_segment_t(uint16_t sequence_number);
	void SendSegment(sim_segment_t *seg,uint32_t now);
	int SendPadding(uint16_t payload_len,uint32_t ts,const webrtc::PacedPacketInfo& pacing_info);
	void SendToNetwork(uint8_t*data,uint32_t len);
	void ProcessingMsg(bin_stream_t *stream);
	void InnerProcessFeedback(sim_feedback_t* feedback);
	void SendPing(int64_t now);
	void UpdateRtt(uint32_t time,int64_t now);
	bool running_{false};
	su_addr dst;
	su_socket fd_{0};
	rtc::CriticalSection buf_mutex_;
	std::map<uint16_t,sim_segment_t*>pending_buf_;
	uint32_t frame_seed_{1};
	uint32_t packet_seed_{1};
	uint16_t trans_seq_{1};
	VideoSource *encoder_{NULL};
	bin_stream_t	stream_;
	webrtc::RtcEventLogNullImpl m_nullLog;
	webrtc::Clock *clock_{NULL};
	webrtc::NetworkControllerFactoryInterface *inner_cf_{NULL};
	std::unique_ptr<webrtc::RtpTransportControllerSend> controller_;
	webrtc:: RtpPacketSender *pacer_{NULL};
	FakeRtpRtcpImpl *rtp_rtcp_{NULL};
	int64_t first_ts_{-1};
	uint32_t uid_{1234};
	uint32_t rtt_{0};
	int64_t sum_rtt_{0};
	int64_t rtt_num_{0};
	int64_t min_rtt_{0};
	int64_t max_rtt_{0};
	int64_t average_rtt_{0};
	int64_t update_ping_ts_{0};
	uint32_t base_seq_=0;
};
}



#endif /* SENDER_H_ */
