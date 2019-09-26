//
// Created by mj on 2019/9/25.
//

#include "LhAVFormat.h"

#define LOGE printf

int work(const char* infile, const char* outfile){
    using namespace videorecorder;
    av_register_all();
    LhAVFormat* pFormat = new LhAVFormat();

    AVStream* pInStream1 = NULL;
    AVStream* pInStream2 = NULL;
    AVStream* pOutStream1 = NULL;
    AVStream* pOutStream2 = NULL;
    AVPacket pkt;


    int ret = 0;
    int inIdx1, inIdx2;
    int outIdx1, outIdx2;
    ret = pFormat->open_input_file(infile);
    if(ret != 0){
        LOGE("open_input_file failed,err:(%d,%s)", ret, av_err2str(ret));
        goto end;
    }

    ret = pFormat->open_output_file(outfile);
    if(ret != 0){
        LOGE("open_output_file failed,err:(%d,%s)", ret, av_err2str(ret));
        goto end;
    }

    inIdx1 = pFormat->find_audio_stream(0);
    if(inIdx1 < 0){
        LOGE("could not find audio stream");
        goto end;
    }

    inIdx2 = pFormat->find_audio_stream(inIdx1 + 1);
    if(inIdx2 < 0){
        LOGE("could not find second audio stream");
        goto end;
    }

    pInStream1 = pFormat->getInFmtCtx()->streams[inIdx1];
    pInStream2 = pFormat->getInFmtCtx()->streams[inIdx2];

    pOutStream1 = pFormat->add_stream_by_stream(pInStream1, true);
    pOutStream2 = pFormat->add_stream_by_stream(pInStream2, true);

    if(!pOutStream1 || !pOutStream2){
        //error
        goto end;
    }

    outIdx1 = pOutStream1->index;
    outIdx2 = pOutStream2->index;

    ret = pFormat->write_mp4_header(true);
    if(ret != 0){
        LOGE("write_mp4_header failed,err:(%d,%s)", ret, av_err2str(ret));
        goto end;
    }

    while (true){
        ret = pFormat->read_packet(&pkt);
        if(ret < 0){
            break;
        }

        if(pkt.stream_index == inIdx1){
            pkt.stream_index = outIdx1;
        } else if(pkt.stream_index == inIdx2){
            pkt.stream_index = outIdx2;
        } else{
            continue;
        }

        ret = pFormat->write_packet(&pkt);
        if(ret < 0){
            LOGE("write_packet failed,err:(%d,%s)", ret, av_err2str(ret));
            goto end;
        }
    }

    ret = pFormat->write_tailer();
    if(ret != 0){
        LOGE("write_tailer failed,err:(%d,%s)", ret, av_err2str(ret));
        goto end;
    }

    ret = 0;
end:
    delete pFormat;

    return ret;
}

int main(){

    work("/Users/mj/fromWindows/90348553.mka", "/Users/mj/fromWindows/90348553.mp4");

    return 0;
}
