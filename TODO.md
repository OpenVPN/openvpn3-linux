Quick TODO list for OpenVPN 3 Linux client
==========================================

This list is not an exhaustive list, but some critical points needed to be
taken care of.

- [+] Implment persistent storage of VPN profiles

  Status: In progress.  An already working alternative is to use the
  [`openvpn3-autoload`](docs/openvpn3-autoload.md) feature.

- [ ] Write man-pages

- [ ] Add proper compile time configuration enabling OpenSSL instead of mbed TLS

- [ ] Write a new NetworkManager plug-in for OpenVPN 3, which should essentially
  just be a D-Bus client interacting with the configuration and session
  managers.

- [ ] Write a GUI tool which can run in the background and can pop-up
  appropriate dialogue boxes whenever the front-end user needs to provide
  user credentials.

- [ ] Implement PKCS#11 support

- [ ] Look into SELinux policies, with the goal that all backend services should
  run with restricted possibilities and their own process context types.
