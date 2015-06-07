/*
 *
 *
 *
 */

// import the thrift headers
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/TToString.h>
#include <thrift/transport/TSocket.h>

// import common utility headers
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <fstream>
#include <sys/time.h>

// import the service headers
#include "ImageMatchingService.h"
#include "detect.h"
#include "/home/momo/Research/sirius/sirius-application/command-center/gen-cpp/CommandCenter.h"
#include "/home/momo/Research/sirius/sirius-application/command-center/gen-cpp/commandcenter_types.h"

// define the namespace
using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;
using namespace cmdcenterstubs;

// define the constant
#define THREAD_WORKS 16

class ImageMatchingServiceHandler : public ImageMatchingServiceIf {
	public:
		// put the model training here so that it only needs to
		// be trained once
		ImageMatchingServiceHandler(){
			this->matcher = new FlannBasedMatcher();
			cout << "building the image matching model..." << endl;
			build_model(this->matcher, &(this->trainImgs));
		}
		
		void match_img(string &response, const string &image){
			gettimeofday(&tp, NULL);
			long int timestamp = tp.tv_sec * 1000 + tp.tv_usec / 1000;
			string image_path = "input-" + to_string(timestamp) + ".jpg";
			ofstream imagefile(image_path.c_str(), ios::binary);
			imagefile.write(image.c_str(), image.size());
			imagefile.close();
			response = exec_match(image_path, this->matcher, &(this->trainImgs));
		}
	private:
		struct timeval tp;
		DescriptorMatcher *matcher;
		vector<string> trainImgs;
};

int main(int argc, char **argv){
  int port = 9090;
  //Register with the command center 
  int cmdcenterport = 8081;
  boost::shared_ptr<TTransport> cmdsocket(new TSocket("localhost", cmdcenterport));
  boost::shared_ptr<TTransport> cmdtransport(new TBufferedTransport(cmdsocket));
  boost::shared_ptr<TProtocol> cmdprotocol(new TBinaryProtocol(cmdtransport));
  CommandCenterClient cmdclient(cmdprotocol);
  cmdtransport->open();
  cout<<"Registering automatic speech recognition server with command center..."<<endl;
  MachineData mDataObj;
  mDataObj.name="localhost";
  mDataObj.port=port;
  cmdclient.registerService("IMM", mDataObj);
  cmdtransport->close();

	// initial the transport factory
	boost::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
	boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(9090));
	// initial the protocal factory
	boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
	// initial the request handler
	boost::shared_ptr<ImageMatchingServiceHandler> handler(new ImageMatchingServiceHandler());
	// initial the processor
	boost::shared_ptr<TProcessor> processor(new ImageMatchingServiceProcessor(handler));
	// initial the thread manager and factory
	boost::shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(THREAD_WORKS);
	boost::shared_ptr<PosixThreadFactory> threadFactory = boost::shared_ptr<PosixThreadFactory>(new PosixThreadFactory());
	threadManager->threadFactory(threadFactory);
	threadManager->start();
	
	// initial the image matching server
	TThreadPoolServer server(processor, serverTransport, transportFactory, protocolFactory, threadManager);

	cout << "Starting the image matching server..." << endl;
	server.serve();
	cout << "Done..." << endl;
	return 0;
}