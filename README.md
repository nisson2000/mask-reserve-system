# mask-reserve-system

## 1. Problem Description
Due to the coronavirus, people are now strongly recommended to wear a mask in public. To make the process of ordering masks go smoothly, you are expected to implement a simplified mask pre-order system: `csieMask`.

The `csieMask` system is composed of read and write servers, both can access a file `preorderRecord` that records infomation of consumer's order. When a server gets a request from clients, it will response according to the content of the file. A read server can tell the client how many masks can be ordered. A write server can modify the file to record the orders.

You are expected to complete the following tasks:
1. Implement the servers. The provided sample source code `server.c` can be compiled as simple read/write servers so you don't have to code them from scratch. Details will be described in the later part.
2. Modify the code so the servers will not be blocked by a single request, but can deal with many requests at the same time. You might want to use `select()` to implement the multiplexing system.
3. Guarantee the correctness of file content when it is being modified. You might want to use file lock to protect the file.

**Read Server**

Clients can check how many masks they can order. 
Once it has connected to a read server, the terminal will show the following:

```shell
Please enter the id (to check how many masks can be ordered):
```

If you type an id (for example, `902001`) on the client side, the server shall reply:


```shell
You can order 10 adult mask(s) and 10 children mask(s).
```
and close the connection from client.


But, if someone else is ordering using the same id, the server shall reply:
```shell
Locked.
```
and close the connection from client.


**Write Server**

A consumer can make preorders. It will first show how many masks a consumer can order just like a read server, then ask for masktype and number of mask to preorder.

Once it connect to a write server, the terminal will show the following:
```shell
Please enter the id (to check how many masks can be ordered):
```
If you type an id (for example, `902001`) on the client side, the server shall reply the numbers, following by a prompt on the nextline.

```shell
You can order 10 adult mask(s) and 10 children mask(s).
Please enter the mask type (adult or children) and number of mask you would like to order:
```
If you type a command (for example, `adult 2`), the server shall modify the `preorderRecord` file and reply:
```
Pre-order for 902001 successed, 2 adult mask(s) ordered.
```
and close the connection from client.

But, if someone else is ordering using the same id, the server shall reply:
```shell
Locked.
```
and close the connection from client.

If the request cannot be process by the servers for other reasons, the servers shall reply:
```shell
Operation failed.
```
and close the connection from client.
