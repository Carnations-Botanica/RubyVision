<h1 align="center">RubyVision</h1>

<h5 align="center">Support for third party ATI/AMD GPUs on OS X / macOS.</h5>
</br>

A [Lilu](https://github.com/acidanthera/Lilu) plug-in kernel extension for ATI/AMD branded graphics processing units, and similar architecture found in OS X and macOS.

</br>
<h1 align="center">Purpose</h1>
</br>

Specifically designed to help debug, and patch various ATI/AMD related kernel extensions present on the OS X and macOS installs to get off the shelf or not natively supported graphics processing unit variants working on non-Apple hardware. The intention is to support the earliest OS X versions first such as Tiger, and work our way up to only very early macOS releases. 

</br>
<h1 align="center">Usage / Features</h1>
</br>

<h3>AtiDbg</h3>

- getProperty
    - Universal XNU hook to log when a process calls for a property request

- Re-routed ATI/AMD functions for debugging and sanity checking

<h3>ATIRadeonX2000</h3>

- Connectors data can be dynamically injected

<h3>ATI/AMD5000Controller</h3>

- Re-implemented various functions to help provide the expected data

- Connectors data can be dynamically injected

</br>
<h1 align="center">Contributing to the Project</h1>

This Project does not take contributions as of right now.

</br>
<h1 align="center">Special Thanks!</h1>

- [RoyalGraphX](https://github.com/RoyalGraphX) - Project Lead, Reverse Engineering, Tools, etc.

- [Goldfish64](https://github.com/Goldfish64) - Information on building i386 slice for OS X Tiger, Leopard.

- [Keneshin](https://github.com/keneshindev) - OS X Leopard + ATI Radeon 2400 XT for testing under [DarwinKVM](https://docs.darwinkvm.com).

<h6 align="center">A big thanks to all contributors and future contributors! ꩓</h6>
