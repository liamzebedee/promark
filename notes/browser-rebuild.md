


What about dapps? 
Can we define a simpler interface to DeFi?
Where all the data sources are authenticated? 
    ie. what is the uniswap dapp?

    swap
        a(asset,amt) b(asset,amt)
        dir buy|sell
        price info



This isn't the objective. The objective is secure decentralised web apps.
The simplest approach for Dappnet would be:
- for now, just an ENS name resolution system for URL's.
- secondly, incorporate IWA's tech when it's ready.
- build a suite for deploying apps using IWA over Vercel or something.
- figure out the next problem: apps being unavailable due to third-party servers:
    - full node local support
    - "local mode" just disable all networking for a single app (except via an RPC node API?)




What if we reimplemented a browser engine? what is the minimum? the 80%
- html5
- css3
- js engine (bun)
- fonts
    - international rendering, rtl
- http networking
    - basic http/1
    - http/2
        - spdy
- dns
- ssl
- engine
    - rendering
    - keyboard
    - event bubbling
    - parsing
    - clipboard
    - mouse
- browser
    - navigation
    - tabs
    - scrolling
- dom api's
    - keyboard
    - url's
    - drag and drop?
- content type rendering
    - html
        - textarea
        - button
    - images (formats)
    - video (formats)
    - svg
- font loading
- web engine stuff
    - cookies
    - headers
    - passkeys

And importantly, whatever React.js uses:
- JS VM: spec-level ES202x (classes, async/await, proxies, modules, BigInt, etc.)
- Event loop + job/microtask queues (Promises) + timers (setTimeout/setInterval)
- Web platform builtins expected by React apps and their deps: URL, fetch, Headers/Request/Response, AbortController, TextEncoder/Decoder, structuredClone, crypto.getRandomValues, performance.now, console, etc.

