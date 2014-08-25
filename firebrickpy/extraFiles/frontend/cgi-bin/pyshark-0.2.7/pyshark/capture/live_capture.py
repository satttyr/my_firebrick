from pyshark.capture.capture import Capture
from pyshark.utils import StoppableThread, StopThread
from pyshark.tshark.tshark import get_tshark_interfaces

class LiveCapture(Capture):
    """
    Represents a live capture on a network interface.
    """

    def __init__(self, interface=None, bpf_filter=None, display_filter=None, only_summaries=False):
        """
        Creates a new live capturer on a given interface. Does not start the actual capture itself.

        :param interface: Name of the interface to sniff on. If not given, takes the first available.
        :param bpf_filter: BPF filter to use on packets.
        :param display_filter: Display (wireshark) filter to use.
        :param only_summaries: Only produce packet summaries, much faster but includes very little information
        """
        super(LiveCapture, self).__init__(display_filter=display_filter, only_summaries=only_summaries)
        self.bpf_filter = bpf_filter
        
        if interface is None:
            self.interfaces = get_tshark_interfaces()
        else:
            self.interfaces = [interface]
    
    def sniff(self, packet_count=None, timeout=None):
        """
        Captures from the set interface, until the given amount of packets is captured or the timeout is reached.
        When using interactively, can be stopped by a Keyboard Interrupt.
        All packets are added to the packet list. Can be called multiple times.

        :param packet_count: an amount of packets to capture, then stop.
        :param timeout: stop capturing after this given amount of time.
        """
        sniff_thread = StoppableThread(target=self._sniff_in_thread, args=(packet_count,))
        try:
            sniff_thread.start()
            sniff_thread.join(timeout=timeout)
            # If the thread is still alive after joining, then it timed out
            if sniff_thread.is_alive():
                sniff_thread.raise_exc(StopThread)
        except KeyboardInterrupt:
            print 'Interrupted, stopping..'
            sniff_thread.raise_exc(StopThread)
    
    def _sniff_in_thread(self, packet_count=None):
        """
        Sniff until stopped and add all packets to the packet list.
        """
        self._set_tshark_process(packet_count)
        try:
            for packet in self.sniff_continuously(packet_count=packet_count):
                self._packets += [packet]
        except StopThread:
            self._cleanup_subprocess()
    
    def sniff_continuously(self, packet_count=None):
        """
        Captures from the set interface, returning a generator which returns packets continuously.

        Can be used as follows:
        for packet in capture.sniff_continuously();
            print 'Woo, another packet:', packet

        :param packet_count: an amount of packets to capture, then stop.
        """
        if self.tshark_process is None:
            self._set_tshark_process(packet_count=packet_count)
        
        for packet in self._packets_from_fd(self.tshark_process.stdout, packet_count=packet_count):
            yield packet
        
        self._cleanup_subprocess()
    
    def get_parameters(self, packet_count=None):
        """
        Returns the special tshark parameters to be used according to the configuration of this class.
        """
        params = super(LiveCapture, self).get_parameters(packet_count=packet_count)
        for interface in self.interfaces:
            params += ['-i', interface]
        if self.bpf_filter:
            params += ['-f', self.bpf_filter]
        return params