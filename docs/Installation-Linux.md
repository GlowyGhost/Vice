Installation may be a bit awkward due to needing a virtual mic. Vice requires you to install a virtual audio device for input and output. There's many options for a virtual mic. The most popular way is using PipeWire.
1. If PipeWire hasn't beem installed, run `sudo apt install pipewire pipewire-pulse`
2. Create the virtual mic:

`pactl load-module module-null-sink sink_name=ViceMic sink_properties=device.description="ViceMic"`

`pactl load-module module-loopback source=ViceMic.monitor`

3. Download Vice.
4. Set Vice's output to **`ViceMic`**
5. In any apps or games you want to use this virtual mic, Set it's input to **`ViceMic`**