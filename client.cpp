#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

#include <thread>
#include <unistd.h>
#include <mutex>
#include <sys/wait.h>
using namespace std;

void patient_thread_function(int n, int pno, BoundedBuffer* request_buffer){
    /* What will the patient threads do? */
    datamsg d(pno, 0, 1);
    for(int i=0; i<n; i++) {
        request_buffer->push((char*)&d, sizeof(datamsg));
        d.seconds += 0.004;
    }
}

struct Response {
    int person;
    double ecg;
};

void worker_thread_function(TCPRequestChannel* chan, BoundedBuffer* request_buffer, BoundedBuffer* response_buffer, int mb){
    char buf[1024];
    double dd = 0;

    char recvbuf [mb];
    while(true) {
        request_buffer->pop(buf, 1024);
        MESSAGE_TYPE* m = (MESSAGE_TYPE *) buf;
        if(*m == DATA_MSG) {
            //datamsg* d = (datamsg*) buf;
            chan->cwrite(buf, sizeof(datamsg));
            //double ecg;
            chan->cread(&dd, sizeof(double));
            Response r{((datamsg*) buf)->person, dd};
            response_buffer->push ((char*) &r, sizeof(r));
            //hc->update (((datamsg*)buf)->person, d);
        } else if (*m ==  FILE_MSG) {
            filemsg* fm = (filemsg* ) buf;
            string fname = (char *) (fm + 1);
            int sz = sizeof(filemsg) + fname.size() + 1;
            chan->cwrite (buf, sz);
            chan->cread (recvbuf, mb);

            // write recvbuf to file
            string recvfname = "recv/" + fname;

            FILE* fp = fopen(recvfname.c_str(), "r+");
            fseek (fp, fm->offset, SEEK_SET);
            fwrite (recvbuf, 1, fm->length, fp);
            fclose(fp);
        } else if(*m == QUIT_MSG) {
            chan->cwrite (m, sizeof(MESSAGE_TYPE));
            delete chan;
            break;
        }
    }
}
void file_thread_function (string fname, BoundedBuffer* request_buffer, TCPRequestChannel* chan, int mb ){
    //if(fname == "") // no file transfer
    //    return;
    char buf [1024];
    filemsg f (0,0);
    memcpy(buf, &f, sizeof(f));
    strcpy (buf + sizeof(f), fname.c_str());
    chan->cwrite (buf, sizeof(f) + fname.size() + 1);
    __int64_t filelength;
    chan->cread (&filelength, sizeof(filelength));

    // Create the file
    string recvfname = "recv/" + fname;
    FILE* fp = fopen(recvfname.c_str(), "w");
    fseek (fp, filelength, SEEK_SET);
    fclose (fp);

    // Generate all of the required file messages
    filemsg* fm = (filemsg*) buf;
    __int64_t remlen = filelength;

    while(remlen > 0){
        fm->length = min (remlen, (__int64_t) mb);
        request_buffer->push (buf, sizeof(filemsg) + fname.size() + 1);
        fm->offset += fm->length;
        remlen -= fm->length;
    }
}


void histogram_thread_function (BoundedBuffer* responseBuffer, HistogramCollection* hc){
    char buf[1024];
    while(true) {
        responseBuffer->pop(buf, 1024);
        Response* r = (Response*) buf;
        if(r->person == -1) {
            break;
        }
        hc->update (r->person, r->ecg);
    }
}


int main(int argc, char *argv[])
{
    int n = 1000;    //default number of requests per "patient"
    int p = 10;     // number of patients [1,15]
    int w = 50;    //default number of worker threads
    int b = 20; 	// default capacity of the request buffer, you should change this default
	int m = MAX_MESSAGE; 	// default capacity of the message buffer
    int h = 3;
    srand(time_t(NULL));
    string fname = "1.csv";
    bool is_filetransfer = false;
    string host;
    string port;

    //Get Opt
    int opt;// =-1;
    while ((opt = getopt(argc, argv, "m:n:p:b:w:f:h:a:r:")) != -1){
        switch(opt){
            case 'm':
                m = atoi(optarg);
                break;
            case 'n':
                n = atoi(optarg);
                break;
            case 'p':
                p = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg);
                break;
            case 'f':
                fname = optarg;
                is_filetransfer = true;
                break;
            case 'h':
                h = atoi(optarg);
                break;
            case 'a':
                host = optarg;
                break;
            case 'r':
                port = optarg;
                break;
        }
    }

    //std::cout<<"Made it"<<endl;
    //int pid = fork();
    /*if (pid == 0){
		// modify this to pass along m
        char* args [] = {"./server", "-m", (char *) to_string(m).c_str(), NULL};
        if (execvp (args [0], args) < 0){
            perror ("exec filed");
            exit (0);
        }
        //execl ("server", "server", (char *)NULL);
    }*/
    //std::cout<<"Made it3"<<endl;
    //std::cout<<host << " " <<port<<endl;
	TCPRequestChannel* chan = new TCPRequestChannel(host,port);
    //std::cout<<"Made it"<<endl;
    BoundedBuffer request_buffer(b);
    //std::cout<<"Made it1"<<endl;
    BoundedBuffer response_buffer(b);
   // std::cout<<"Made it2"<<endl;
	HistogramCollection hc;

//std::cout<<"Made it"<<endl;
    char name[1024];
    TCPRequestChannel* wchans[w];
    //std::cout<<"Made it"<<endl;
    for(int i=0; i<w; i++) {
        //MESSAGE_TYPE m = NEWCHANNEL_MSG;
        //chan->cwrite(&m, sizeof(m));
        //chan->cread(&name, 1024);
        wchans[i] = new TCPRequestChannel(host,port);
        //wchans[i] = newchan;
    }

    for(int i=0; i<p; i++) {
        Histogram *h = new Histogram(10,-2,2);
        hc.add(h);
    }
	
    struct timeval start, end;
    gettimeofday (&start, 0);
    //std::cout<<"Made it"<<endl;
    if (is_filetransfer) {
        thread filethread (file_thread_function, fname, &request_buffer,  chan,  m);
        thread worker [w];
        for(int i=0; i<w; i++) {
            worker[i] = thread (worker_thread_function, wchans[i], &request_buffer, &response_buffer, m);
        }
        sleep(1);
        for(int i=0; i<w; i++) {
            MESSAGE_TYPE q = QUIT_MSG;
            request_buffer.push((char*) &q, sizeof(q));
        }
        //Join File Thread
        filethread.join();
        for(int i=0; i<w; i++) {
            worker[i].join();
        }
    } else {
        thread patient [p];
        for(int i=0; i<p; i++) {
            patient[i] = thread (patient_thread_function, n, i+1, &request_buffer);
        }
        thread worker [w];
        for(int i=0; i<w; i++) {
            worker[i] = thread (worker_thread_function, wchans[i], &request_buffer, &response_buffer, m);
        }
        thread hists [h];
        for(int i=0; i<h; i++) {
            hists[i] = thread (histogram_thread_function, &response_buffer, &hc);
        }

        /* Join all threads here */
        for(int i=0; i<p; i++) {
            patient[i].join();
        }
        //
        sleep(1);
        for(int i=0; i<w; i++) {
            MESSAGE_TYPE q = QUIT_MSG;
            request_buffer.push((char*) &q, sizeof(q));
        }
        for(int i=0; i<w; i++) {
            worker[i].join();
        }
        
        Response r {-1, 0};
        for(int i=0; i<h; i++) {
            response_buffer.push((char*)&r, sizeof(r));
        }
        for(int i=0; i<h; i++) {
            hists[i].join();
        }
    }

	//std::cout<<"Made it"<<endl;
    gettimeofday (&end, 0);
    // print the results
	hc.print ();
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    wait(0);
    delete chan;
    
}
