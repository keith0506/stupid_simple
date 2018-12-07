#include "fakevideosource.h"
#include <unistd.h>
#include "rtc_base/timeutils.h"
namespace zsy{
VideoGenerator::VideoGenerator(rtc::TaskQueue *w,uint32_t fs,uint32_t minR){
	w_=w;
	fs_=fs;
	rate_=minR;
	minR_=minR;
}
void VideoGenerator::Start(){
	running_=true;
	Generate();
}
void VideoGenerator::Stop(){
	running_=false;
}
void VideoGenerator::RegisterSender(SendInterface *sender){
	sender_=sender;
}
void VideoGenerator::ChangeRate(uint32_t bitrate){
	if(bitrate>minR_){
		rate_=bitrate;
	}else{
		rate_=minR_;	
	}
	if(log_enable_){
		LogRate(rate_);
	}
}
void VideoGenerator::SetLogFile(std::string name){
	char buf[FILENAME_MAX];
	memset(buf,0,FILENAME_MAX);
	std::string path = std::string (getcwd(buf, FILENAME_MAX))
			+name+"_rate.txt";
	log_enable_=true;
	f_rate_.open(path.c_str(), std::fstream::out);
}
void VideoGenerator::Close(){
	if(f_rate_.is_open()){
		f_rate_.close();
	}
}
void VideoGenerator::SendFrame(){
	float len=(float)rate_/(fs_ * 8.f);
	uint32_t bytesToSend=len;
	if(sender_){
		uint8_t *buf=new uint8_t[bytesToSend];
		sender_->SendVideo(0,0,buf,bytesToSend);
		delete buf;
	}
}
void VideoGenerator::Generate(){
	if(running_){
		SendFrame();
		uint32_t duration=1000/fs_;
		w_->PostDelayedTask([this]{
			this->Generate();
		},duration);
	}
}
void VideoGenerator::LogRate(uint32_t bitrate){
	uint32_t now=rtc::TimeMillis();
	if(first_==0){
		first_=now;
	}
	uint32_t abs=now-first_;
	time_rate_t record;
	record.now=abs;
	record.bps=bitrate;
	w_->PostTask([this,record]{
		this->WriteToFile(record);
	});
}
void VideoGenerator::WriteToFile(time_rate_t record){
	if(f_rate_.is_open()){
		char line [256];
		memset(line,0,256);
		uint32_t abs=record.now;
		float kbps=(float)record.bps/1000.0;
		sprintf (line, "%d,%16f",abs,kbps);
		f_rate_<<line<<std::endl;
	}
}
}






