#ifndef MYBBR_MY_BBR_VIDEO_SENDER_APP_H_
#define MYBBR_MY_BBR_VIDEO_SENDER_APP_H_
#include "my_pacing_sender.h"
#include "my_quic_framer.h"
#include "my_quic_clock.h"
#include "socket.h"
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
namespace net{
class FakeVideoGenerator;
class MyBbrVideoSenderApp{
public:
	MyBbrVideoSenderApp(uint64_t min_bps,
			uint64_t start_bps,
			uint64_t max_bps);
	~MyBbrVideoSenderApp();
	bool Process();
	void set_socket(zsy::Socket *socket) { socket_=socket;}
	void set_peer(su_addr peer) { peer_=peer; }
	void set_duration(uint32_t ms);
	void EnableRateRecord(std::string name);
    void SetTimeOffset(int64_t offset){
        offset_=offset;
    }
	void SetVideoSource(FakeVideoGenerator *source){
		source_=source;
	}
private:
    void RecordRate(QuicTime now);
    void SendDataOrPadding(QuicTime now);
    void SendFakePacket(QuicTime now);
    void SendPaddingPacket(QuicTime now);
    void OnPacketSent(QuicPacketNumber packet_number,
    		QuicPacketLength payload_len);
    void ProcessAckFrame(QuicTime now,uint8_t *data,int len);
	zsy::Socket *socket_{NULL};
	MyQuicClock clock_;
	su_addr peer_;
	uint32_t seq_{1};
	QuicTime next_;
	QuicTime ref_time_;
	QuicTime stop_;
	bool enable_log_{false};
	std::fstream f_rate_;
    int64_t offset_{0};
    MyPacingSender pacing_;
	MyBbrSender cc_;
    uint64_t sent_count_{0};
    FakeVideoGenerator *source_{NULL};
    std::vector<int> pending_queue_;
};
}




#endif /* MYBBR_MY_BBR_VIDEO_SENDER_APP_H_ */
