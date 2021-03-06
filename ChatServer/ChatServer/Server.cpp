#include "Server.h"

Server::Server(int PORT, bool BroadcastPublically) //Port = port to broadcast on. BroadcastPublically = false if server is not open to the public (people outside of your router), true = server is open to everyone (assumes that the port is properly forwarded on router settings)
{
	//Winsock Startup
	WSAData wsaData;
	WORD DllVersion = MAKEWORD(2, 1);
	if (WSAStartup(DllVersion, &wsaData) != 0) //If WSAStartup returns anything other than 0, then that means an error has occured in the WinSock Startup.
	{
		MessageBoxA(NULL, "WinSock startup failed", "Error", MB_OK | MB_ICONERROR);
		exit(1);
	}

	if (BroadcastPublically) //If server is open to public
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	else //If server is only for our router
		addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //Broadcast locally
	addr.sin_port = htons(PORT); //Port
	addr.sin_family = AF_INET; //IPv4 Socket

	sListen = socket(AF_INET, SOCK_STREAM, NULL); //Create socket to listen for new connections
	if (::bind(sListen, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) //Bind the address to the socket, if we fail to bind the address..
	{
		std::string ErrorMsg = "Failed to bind the address to our listening socket. Winsock Error:" + std::to_string(WSAGetLastError());
		MessageBoxA(NULL, ErrorMsg.c_str(), "Error", MB_OK | MB_ICONERROR);
		exit(1);
	}
	if (listen(sListen, SOMAXCONN) == SOCKET_ERROR) //Places sListen socket in a state in which it is listening for an incoming connection. Note:SOMAXCONN = Socket Oustanding Max connections, if we fail to listen on listening socket...
	{
		std::string ErrorMsg = "Failed to listen on listening socket. Winsock Error:" + std::to_string(WSAGetLastError());
		MessageBoxA(NULL, ErrorMsg.c_str(), "Error", MB_OK | MB_ICONERROR);
		exit(1);
	}
	serverptr = this;
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)PacketSenderThread, NULL, NULL, NULL); //Create thread that will manage all outgoing packets
}

bool Server::ListenForNewConnection()
{
	SOCKET newConnectionSocket = accept(sListen, (SOCKADDR*)&addr, &addrlen); //Accept a new connection
	if (newConnectionSocket == 0) //If accepting the client connection failed
	{
		std::cout << "Failed to accept the client's connection." << std::endl;
		return false;
	}
	else //If client connection properly accepted
	{
		std::lock_guard<std::mutex> lock(connectionMgr_mutex); //Lock connection manager mutex since we are adding an element to connection vector
		int NewConnectionID = connections.size(); //default new connection id to size of connections vector (we will change it if we can reuse an unused connection)
		if (UnusedConnections > 0) //If there is an unused connection that this client can use
		{
			for (size_t i = 0; i < connections.size(); i++) //iterate through the unused connections starting at first connection
			{
				if (connections[i]->ActiveConnection == false) //If connection is not active
				{
					connections[i]->socket = newConnectionSocket;
					connections[i]->ActiveConnection = true;
					NewConnectionID = i;
					UnusedConnections -= 1;
					break;
				}
			}
		}
		else //If no unused connections available... (add new connection to the socket)
		{
			Connection * newConnection(new Connection(newConnectionSocket));
			connections.push_back(newConnection); //push new connection into vector of connections
		}
		std::cout << "Client Connected! ID:" << NewConnectionID << std::endl;
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ClientHandlerThread, (LPVOID)(NewConnectionID), NULL, NULL); //Create Thread to handle this client. The index in the socket array for this thread is the value (i).
		return true;
	}
}

bool Server::ProcessPacket(int ID, PacketType _packettype)
{
	switch (_packettype)
	{
	case PacketType::ChatMessage: //Packet Type: chat message
	{
									  std::string message; //string to store our message we received
									  if (!GetString(ID, message)) //Get the chat message and store it in variable: Message
										  return false; //If we do not properly get the chat message, return false
									  //Next we need to send the message out to each user
									  //string id = to_string(ID);
									  for (size_t i = 0; i < connections.size(); i++)
									  {
										  if (connections[i]->ActiveConnection == false) //if connection is not active
											  continue; //Skip logic for this connection
										  if (i == ID) //If connection is the user who sent the message...
											  continue;//Skip to the next user since there is no purpose in sending the message back to the user who sent it.
										  
										  SendString(i, data_ptr->retrieve(ID)+" : " + message); //send message to connection i
									  }
									  std::cout << "Processed chat message packet from user ID: " << ID <<std::endl;
									  break;
	}
	case PacketType::RequestChat:
	{
								string message;
								if (!GetString(ID, message)) //Get the chat message and store it in variable: Message
									return false;
								cout << message << endl;
								string message1 = "";
								message1 = message1 + to_string(ID)+" ";
								int id = stoi(message);
								string temp = data_ptr->retrieve(id);
								//cout << temp << endl;
								temp = temp +"!!"+ data_ptr->retrieve_pass(temp);
								message1 += temp + " extra";
								SendStringRC(id,message1);
								Sleep(20);
								cout << "YE WALA";
								break;
								
	}
	case PacketType::ReplyChat:
	{
									string message;
									if (!GetString(ID, message)) //Get the chat message and store it in variable: Message
										return false;
									cout << message << endl;
									string message1 = "";
									message1 = message1 + to_string(ID) + " ";
									int id = stoi(message);
									string temp = data_ptr->retrieve(id);
									//cout << temp << endl;
									//temp = temp + "!!" + data_ptr->retrieve_pass(temp);
									//message1 += temp + " extra";
									SendStringRRC(id, message1);
									Sleep(20);
									cout << "YE WALA";
									break;

	}
	case PacketType::SingleChatMessage:
	{
										  string message;
										  if (!GetString(ID, message)) //Get the chat message and store it in variable: Message
											  return false;
										  //cout << message << endl;
										  string id;
										  if (!GetString(ID, id)) //Get the chat message and store it in variable: Message
											  return false;
										  int Id = stoi(id);
										  
										  string temp = data_ptr->retrieve(ID);
										  temp = temp + ": " + message;
										  if (data_ptr->get_data(Id))
											  SendStringSCM(Id, temp);
										  else{
											  //DisconnectClient(ID);
											  //cout << "What??" << endl;
											  return false;
										  }
										  Sleep(20);
										  break;

	
	}
	case PacketType::RequestUser:
	{
									string users="";
									string message;
									if (!GetString(ID, message)) //Get the chat message and store it in variable: Message
										return false;
									/*for (map<int, string>::iterator it = data_ptr->name_map.begin(); it != data_ptr->name_map.end(); it++){
										users =users+"!"+ it->second;
									}*/
									users = data_ptr->retrieve(ID,0);
									SendStringRU(ID, users);
									Sleep(20);
									break;

	}
	case PacketType::FileTransferRequestFile:
	{
												std::string FileName; //string to store file name
												if (!GetString(ID, FileName)) //If issue getting file name
													return false; //Failure to process packet

												connections[ID]->file.infileStream.open(FileName, std::ios::binary | std::ios::ate); //Open file to read in binary | ate mode. We use ate so we can use tellg to get file size. We use binary because we need to read bytes as raw data
												if (!connections[ID]->file.infileStream.is_open()) //If file is not open? (Error opening file?)
												{
													std::cout << "Client: " << ID << " requested file: " << FileName << ", but that file does not exist." << std::endl;
													std::string ErrMsg = "Requested file: " + FileName + " does not exist or was not found.";
													SendString(ID, ErrMsg); //Send error msg string to client
													return true;
												}

												connections[ID]->file.fileName = FileName; //set file name just so we can print it out after done transferring
												connections[ID]->file.fileSize = connections[ID]->file.infileStream.tellg(); //Get file size
												connections[ID]->file.infileStream.seekg(0); //Set cursor position in file back to offset 0 for when we read file
												connections[ID]->file.fileOffset = 0; //Update file offset for knowing when we hit end of file

												if (!HandleSendFile(ID)) //Attempt to send byte buffer from file. If failure...
													return false;
												break;
	}
	case PacketType::FileTransferRequestNextBuffer:
	{
													  if (!HandleSendFile(ID)) //Attempt to send byte buffer from file. If failure...
														  return false;
													  break;
	}
	case PacketType::RegisterPacket:
	{
									   std::string message; //string to store our message we received
									   if (!GetString(ID, message)) //Get the chat message and store it in variable: Message
										   return false; //If we do not properly get the chat message, return false
									   //Next we need to send the message out to each user
									   if (data_ptr->look_up(message)){
										   string Err = "Username exists";
										   SendStringRR(ID, Err);
									   }
									   else{
										   data_ptr->insert(ID,message);
										   string Err = "Successfully Registered";
										   SendStringRR(ID, Err);
										   Sleep(20);
										   cout << ID << " " << data_ptr->extract_name(message) << "\n";
										   //data_ptr->name_map[ID] = data_ptr->extract_name(message);
									   }

									   break;

	}
	case PacketType::LoginPacket:
	{
									std::string message; //string to store our message we received
									if (!GetString(ID, message)) //Get the chat message and store it in variable: Message
										return false; //If we do not properly get the chat message, return false
									//Next we need to send the message out to each user
									if (!data_ptr->login_check(ID,message)){
										string Err = "Incorrect Username/Password";
										SendStringRL(ID, Err);

									}
									else{
										string name = data_ptr->extract_name(message);
										string Err = "Welcome!!" + name;
										SendStringRL(ID, Err);
										//data_ptr->name_map[ID] = name;
										data_ptr->insert_name(ID, message);
									}
									break;
	}
	default: //If packet type is not accounted for
	{
				 std::cout << "Unrecognized packet: " << (int32_t)_packettype << std::endl; //Display that packet was not found
				 break;
	}
	}
	return true;
}

bool Server::HandleSendFile(int ID)
{
	if (connections[ID]->file.fileOffset >= connections[ID]->file.fileSize) //If end of file reached then return true and skip sending any bytes
		return true;

	connections[ID]->file.remainingBytes = connections[ID]->file.fileSize - connections[ID]->file.fileOffset; //calculate remaining bytes
	if (connections[ID]->file.remainingBytes > connections[ID]->file.buffersize) //if remaining bytes > max byte buffer
	{
		PS::FileDataBuffer fileData; //Create FileDataBuffer packet structure
		connections[ID]->file.infileStream.read(fileData.databuffer, connections[ID]->file.buffersize); //read in max buffer size bytes
		fileData.size = connections[ID]->file.buffersize; //Set FileDataBuffer buffer size
		connections[ID]->file.fileOffset += connections[ID]->file.buffersize; //increment fileoffset by # of bytes written
		connections[ID]->pm.Append(fileData.toPacket()); //Append fileData packet to packet manager
	}
	else
	{
		PS::FileDataBuffer fileData; //Create FileDataBuffer packet structure
		connections[ID]->file.infileStream.read(fileData.databuffer, connections[ID]->file.remainingBytes); //read in remaining bytes
		fileData.size = connections[ID]->file.remainingBytes; //Set FileDataBuffer remaining bytes
		connections[ID]->file.fileOffset += connections[ID]->file.remainingBytes; //increment fileoffset by # of bytes written
		connections[ID]->pm.Append(fileData.toPacket()); //Append fileData packet to packet manager
	}

	if (connections[ID]->file.fileOffset == connections[ID]->file.fileSize) //If we are at end of file
	{
		Packet EOFPacket(PacketType::FileTransfer_EndOfFile); //Create packet to be appended for end of file
		connections[ID]->pm.Append(EOFPacket); //Append end of file packet to packet manager for this client connection
		std::cout << std::endl << "File sent: " << connections[ID]->file.fileName << std::endl;
		std::cout << "File size(bytes): " << connections[ID]->file.fileSize << std::endl << std::endl;
	}
	return true;
}

void Server::ClientHandlerThread(int ID) //ID = the index in the SOCKET connections array
{
	PacketType packettype;
	while (true)
	{
		if (!serverptr->GetPacketType(ID, packettype)) //Get packet type
			break; //If there is an issue getting the packet type, exit this loop
		if (!serverptr->ProcessPacket(ID, packettype)) //Process packet (packet type)
			break; //If there is an issue processing the packet, exit this loop
	}
	std::cout << "Lost connection to client ID: " << ID << std::endl;
	serverptr->DisconnectClient(ID); //Disconnect this client and clean up the connection if possible
	return;
}

void Server::PacketSenderThread() //Thread for all outgoing packets
{
	while (true)
	{
		for (size_t i = 0; i < serverptr->connections.size(); i++) //for each connection...
		{
			if (serverptr->connections[i]->pm.HasPendingPackets()) //If there are pending packets for this connection's packet manager
			{
				Packet p = serverptr->connections[i]->pm.Retrieve(); //Retrieve packet from packet manager
				if (!serverptr->sendall(i, p.buffer, p.size)) //send packet to connection
				{
					std::cout << "Failed to send packet to ID: " << i << std::endl; //Print out if failed to send packet
				}
				delete p.buffer; //Clean up buffer from the packet p
			}
		}
		Sleep(5);
	}
}

void Server::DisconnectClient(int ID) //Disconnects a client and cleans up socket if possible
{
	std::lock_guard<std::mutex> lock(connectionMgr_mutex); //Lock connection manager mutex since we are possible removing element(s) from the vector
	data_ptr->set_data(ID);
	if (connections[ID]->ActiveConnection == false) //If connection has already been disconnected?
	{
		return; //return - this should never happen, but just in case...
	}
	connections[ID]->pm.Clear(); //Clear out all remaining packets in queue for this connection
	connections[ID]->ActiveConnection = false; //Update connection's activity status to false since connection is now unused
	//map<int, string>::iterator it;
	//it = data_ptr->name_map.find(ID);
	data_ptr->data_erase(ID);
	closesocket(connections[ID]->socket); //Close the socket for this connection
	if (ID == (connections.size() - 1)) //If last connection in vector.... (we can remove it)
	{
		connections.pop_back(); //Erase last connection from vector
		//After cleaning up that connection, check if there are any more connections that can be erased (only connections at the end of the vector can be erased)

		for (size_t i = connections.size() - 1; i >= 0 && connections.size()>0; i--)
		{
			if (connections[i]->ActiveConnection) //If connection is active we cannot remove any more connections from vector
				break;
			//If we have not broke out of the for loop, we can remove the current indexed connection
			connections.pop_back(); //Erase last connection from vector
			UnusedConnections -= 1;
		}
	}
	else
	{
		UnusedConnections += 1;
	}
}