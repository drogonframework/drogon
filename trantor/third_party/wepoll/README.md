## wepoll

This library is based on [wepoll v1.5.8](https://github.com/piscisaureus/wepoll/commit/0598a791bf9cbbf480793d778930fc635b044980).

An eventfd-like mechanism is added to it. After making the changes, we can wake up `trantor::EventLoop` from the epoll_wait() function.

## Modifications

```shell
diff wepoll.h Wepoll.h
53a54
>     EPOLLEVENT = (int)(1U << 14),
67a69
> #define EPOLLEVENT (1U << 14)
111a114
>     WEPOLL_EXPORT void epoll_post_signal(HANDLE ephnd, uint64_t event);
```

```shell
diff wepoll.c Wepoll.c
50a51
>     EPOLLEVENT = (int)(1U << 14),
64a66
> #define EPOLLEVENT (1U << 14)
1262a1265,1271
>         if (iocp_events[i].lpCompletionKey)
>         {
>             struct epoll_event* ev = &epoll_events[epoll_event_count++];
>             ev->data.u64 = (uint64_t)iocp_events[i].lpCompletionKey;
>             ev->events = EPOLLEVENT;
>             continue;
>         }
2441a2451,2457
> void epoll_post_signal(HANDLE port_handle, uint64_t event)
> {
>     ULONG_PTR ev;
>     ev = (ULONG_PTR)event;
>     PostQueuedCompletionStatus(port_handle, 1, ev, NULL);
> }
> 
```
