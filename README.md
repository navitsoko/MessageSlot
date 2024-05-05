# MessageSlot
Message Slot in Kernel Module on Linux.

Implementation of a kernel module that provides a new IPC mechanism,
called a message slot.

A message slot is a character device file through which processes communicate.
A message slot device has multiple message channels active concurrently, which can be used by
multiple processes. 

After opening a message slot device file, a process uses ioctl() to specify the
id of the message channel it wants to use. It subsequently uses read()/write() to receive/send
messages on the channel. 
A message channel preserves a message until it
is overwritten, so the same message can be read multiple times.

# Description
1. message_slot: A kernel module implementing the message slot IPC mechanism.
2. message_sender: A user space program to send a message.
3. message_reader: A user space program to read a message.
   
# Compile 
Compile with the following command:

gcc -O3 -Wall -std=c11 message_sender.c (or message_reader.c)


