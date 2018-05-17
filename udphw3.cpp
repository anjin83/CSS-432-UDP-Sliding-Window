#include <iostream>
#include "stdlib.h"
#include "UdpSocket.h"
#include "Timer.h"

using namespace std;

#define MSGSIZE 1460	// Message size is 1460 bytes
#define PORT 23460       // my UDP port
#define MAX 20000        // times of message transfer
#define MAXWIN 30        // the maximum window size
#define LOOP 10          // loop in test 4 and 5

// client packet sending functions
void clientUnreliable( UdpSocket &sock, const int max, int message[] );

// You must implement the following two functions
int clientStopWait( UdpSocket &sock, const int max, int message[] );
int clientSlidingWindow( UdpSocket &sock, const int max, int message[], 
			  int windowSize);

// server packet receiving fucntions
void serverUnreliable( UdpSocket &sock, const int max, int message[] );

// You must implement the following two functions
void serverReliable( UdpSocket &sock, const int max, int message[] );

void serverEarlyRetrans( UdpSocket &sock, const int max, int message[], 
			 int windowSize);


enum myPartType { CLIENT, SERVER, ERROR } myPart;

int main( int argc, char *argv[] ) {

  int message[MSGSIZE/4]; // prepare a 1460-byte message: 1460/4 = 365 ints;
  UdpSocket sock( PORT );  // define a UDP socket

  myPart = ( argc == 1 ) ? SERVER : CLIENT;

  if ( argc != 1 && argc != 2 ) {
    cerr << "usage: " << argv[0] << " [serverIpName]" << endl;
    return -1;
  }

  if ( myPart == CLIENT ) // I am a client and thus set my server address
    if ( sock.setDestAddress( argv[1] ) == false ) {
      cerr << "cannot find the destination IP name: " << argv[1] << endl;
      return -1;
    }

  int testNumber;
  cout << "Choose a testcase" << endl;
  cout << "   1: unreliable test" << endl;
  cout << "   2: stop-and-wait test" << endl;
  cout << "   3: sliding windows" << endl;
  cout << "   4: sliding window with packet drop" << endl;
  cout << "--> ";
  cin >> testNumber;

  if ( myPart == CLIENT ) {

    Timer timer;           // define a timer
    int retransmits = 0;   // # retransmissions
    int dropPercent = 0;	   // drop percentage

    switch( testNumber ) {
    case 1:
      timer.start( );                                          // start timer
      clientUnreliable( sock, MAX, message );                  // actual test
      cerr << "Elasped time = ";                               // lap timer
      cout << timer.lap( ) << endl;
      break;
    case 2:
      timer.start( );                                          // start timer
      retransmits = clientStopWait( sock, MAX, message );      // actual test
      cerr << "Elasped time = ";                               // lap timer
      cout << timer.lap( ) << endl;
      cerr << "retransmits = " << retransmits << endl;
      break;
    case 3:
      for ( int windowSize = 1; windowSize <= MAXWIN; windowSize++ ) {
	timer.start( );                                        // start timer
	retransmits =
	clientSlidingWindow( sock, MAX, message, windowSize ); // actual test
	cerr << "Window size = ";                              // lap timer
	cout << windowSize << " ";
	cerr << "Elasped time = "; 
	cout << timer.lap( ) << endl;
	cerr << "retransmits = " << retransmits << endl;
      }
      break;
    default:
      cerr << "no such test case" << endl;
      break;
    }
  }
  if ( myPart == SERVER ) {
    switch( testNumber ) {
    case 1:
      serverUnreliable( sock, MAX, message );
      break;
    case 2:
      serverReliable( sock, MAX, message );
      break;
    case 3:
      for ( int windowSize = 1; windowSize <= MAXWIN; windowSize++ )
	serverEarlyRetrans( sock, MAX, message, windowSize );
      break;
    default:
      cerr << "no such test case" << endl;
      break;
    }

    // The server should make sure that the last ack has been delivered to
    // the client. Send it three time in three seconds
    cerr << "server ending..." << endl;
    for ( int i = 0; i < 10; i++ ) {
      sleep( 1 );
      int ack = MAX - 1;
      sock.ackTo( (char *)&ack, sizeof( ack ) );
    }
  }

  cerr << "finished" << endl;

  return 0;
}

// Test 1: client unreliable message send -------------------------------------
void clientUnreliable( UdpSocket &sock, const int max, int message[] ) {
  cerr << "client: unreliable test:" << endl;

  // transfer message[] max times
  for ( int i = 0; i < max; i++ ) {
    message[0] = i;                            // message[0] has a sequence #
    sock.sendTo( ( char * )message, MSGSIZE ); // udp message send
    cerr << "message = " << message[0] << endl;
  }
}

// Test 2: client stop and wait message send ----------------------------------

int clientStopWait( UdpSocket &sock, const int numSends, int message[] ) {
	Timer theTimer;
	int counter = 0;	//Keep track of retransmits
	for ( int i = 0; i < numSends; i++ ) {
			 			
	    message[0] = i;             // message[0] has a sequence #
	    int receive[MSGSIZE/4];
	    sock.sendTo( ( char * )message, MSGSIZE ); // udp message send
	    theTimer.start();
	    while(true) {
			if(sock.pollRecvFrom() > 0){	//check if there is a message to read
				sock.recvFrom(( char * )receive, MSGSIZE );
				if(receive[0] == i)//is receive the same as i?
				{
					break;
				}
				else {	//retransmit message
					counter++;
					sock.sendTo( ( char * )message, MSGSIZE );
					theTimer.start();
				}
			}
			else if(theTimer.lap() > 1500){	//if more than 1500 uSecs have passed, retransmit
				counter++;
				sock.sendTo( ( char * )message, MSGSIZE );
				theTimer.start();
			}				
		}
	}
	return counter;
}

// Test 3: client sliding window message send -------------------------------------

int clientSlidingWindow( UdpSocket &sock, const int numSends, int message[], 
			  int windowSize) {
	Timer theTimer;
	int counter = 0;
	int cumack = 0;	//cumulative ACK
	int lastPacketSent = 0;
	int rec[MSGSIZE/4];
	cerr << "client sliding window test:" << endl;
	while(cumack < numSends-1){

		while((lastPacketSent - cumack) <= windowSize && lastPacketSent < numSends){//while there is room in the window, send packets
			message[0] = lastPacketSent++;	//send the current packet, then prepare the next packet
			sock.sendTo( ( char * )message, MSGSIZE );

			if(sock.pollRecvFrom() > 0){
				sock.recvFrom( ( char * ) rec, MSGSIZE );   // udp message receive
				if(rec[0] >= cumack)cumack = rec[0];		
			}
		}
		theTimer.start();
		while(1){
			if(sock.pollRecvFrom() > 0){
				sock.recvFrom( ( char * ) rec, MSGSIZE );   // udp message receive
				if(rec[0] >= cumack)cumack = rec[0];
				break;
			}
			else if(theTimer.lap() > 1500){	//timeout for ack, retransmit all packets in the window
				counter += (lastPacketSent - cumack);	//add the packets retransmitted to the counter	
				lastPacketSent = cumack + 1;
				break;
			}
		}
	}
 
	//initiate 3 way handshake with server to end communication
	int buf[MSGSIZE/4];
	int windowSz = -1;
	while(windowSz == -1)
	{
		while(sock.pollRecvFrom() <= 0){
	
		}
		sock.recvFrom( ( char * ) &windowSz, sizeof(windowSize));
	}
	int clientAck = -1;
	
	while(clientAck == -1)
	{
		theTimer.start();
		sock.ackTo((char *) &windowSz, sizeof(windowSize));
		while(sock.pollRecvFrom() <= 0)
		{
			if(theTimer.lap() > 1500)
			{   
				theTimer.start();
				sock.ackTo((char *) &windowSz, sizeof(windowSize));
			}
		}
		sock.recvFrom( (char *)&clientAck, sizeof(clientAck));
	}
	
	return counter;
}

// Test1: server unreliable message receive -----------------------------------
void serverUnreliable( UdpSocket &sock, const int max, int message[] ) {
  cerr << "server unreliable test:" << endl;

  // receive message[] max times
  for ( int i = 0; i < max; i++ ) {
    sock.recvFrom( ( char * ) message, MSGSIZE );   // udp message receive
    cerr << message[0] << endl;                     // print out message
  }
}

// Test2: server stop and wait message receive -----------------------------------
void serverReliable( UdpSocket &sock, const int max, int message[] ) {
    cerr << "server stop and wait test:" << endl;
		int nextSequence = 0;
		while( nextSequence < max ) {
				if(sock.pollRecvFrom() > 0){//is there something to read
					sock.recvFrom( ( char * ) message, MSGSIZE );// udp message receive
					if(message[0] == nextSequence)
						nextSequence++;
					sock.ackTo( ( char * )message, MSGSIZE );
				}
		}

}

// Test3: server sliding window message receive -----------------------------------

void serverEarlyRetrans( UdpSocket &sock, const int max, int message[], 
             int windowSize) {//packetDrop is the percentage of packets to drop.
    cerr << "server sliding window test:" << endl;
    Timer theTimer;
    bool ackArray[max];
    for(int i =0; i < max; i++)
    {
        ackArray[i] = false;
    }
    int cumAck = 0;
    while(cumAck < max)
    {
        if(sock.pollRecvFrom() > 0)
        {
	    sock.recvFrom( (char *) message, MSGSIZE ); // receive the packet
            int currentPacket = message[0];
            message[0] = cumAck;	
            sock.ackTo( (char *)message, MSGSIZE );
  	    cumAck++; 	      
        }
    }

    int clientAck = 0;
    int clientReceived = -1;
    while(clientReceived == -1)
    {
        sock.ackTo( (char *)&windowSize, sizeof(windowSize));
	      theTimer.start();
        while(sock.pollRecvFrom() <= 0)  //client sent an ack
        {
            if(theTimer.lap() > 1500)
      	    {
 		            sock.ackTo( (char *)&windowSize, sizeof(windowSize));
      	    	  theTimer.start();
      	    }          
        }
	      sock.recvFrom( (char *) &clientReceived, sizeof(clientReceived));
    }
    sock.ackTo( (char *)&windowSize, sizeof(windowSize));
}

