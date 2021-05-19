use crate::recovery;
// MP: Use HashMAP
use std::collections::HashMap;
// use crate::Result;
// use std::time;
use crate::Config;

// const MAX_HASHMAP_CAPACITY: usize = 1000000;

pub struct Path {
    // QUIC wire version used for the connection.
    // version: u32,

    // Path ID
    // path_id: u8,

    // Peer's path connection ID.
    pub dcid: Vec<u8>,

    // Local path connection ID.
    pub scid: Vec<u8>,

    // seq map : [packet_num : seq]
    pub pkts_num_with_seq: HashMap<u64, u64>,

    /// previous send stream.
    pub last_block: u64,

    pub previous_unsend: u64,

    pub previous_bw: f64,

    pub previous_rtt: u128, //ms

    // Unique opaque ID for the connection that can be used for logging.
    // trace_id: String,

    // Packet number spaces.
    // Each path keeps its own monotonically increasing Packet Number space
    // 同一个connection不同path共用一个space空间，使用SACK实现，sender知道哪些到了哪些丢了就行
    // pkt_num_spaces: [packet::PktNumSpace; packet::EPOCH_COUNT],

    /* ******* Connection Use ************

    // Peer's transport parameters.
    peer_transport_params: TransportParams,

    // Local transport parameters.
    local_transport_params: TransportParams,

    // TLS handshake state.
    // handshake: tls::Handshake,

     ******* Connection Use ************ */
    // Loss recovery and congestion control state.
    pub recovery: recovery::Recovery,
    // List of supported application protocols.
    // application_protos: Vec<Vec<u8>>,

    // Total number of received packets.
    // recv_count: usize,

    // Total number of sent packets.
    // sent_count: usize,

    // Total number of bytes received from the peer.
    // rx_data: u64,

    // Local flow control limit for the connection.
    // path level flow control
    // max_rx_data: u64,

    // Updated local flow control limit for the connection. This is used to
    // trigger sending MAX_DATA frames after a certain threshold.
    // max_rx_data_next: u64,

    // Total number of bytes sent to the peer.
    // tx_data: u64,

    // Peer's flow control limit for the connection.
    // max_tx_data: u64,

    // Total number of bytes the server can send before the peer's address
    // is verified.
    // max_send_bytes: usize,

    // Streams map, indexed by stream ID.
    // streams: stream::StreamMap,

    // Peer's original connection ID. Used by the client during stateless
    // retry to validate the server's transport parameter.
    // odcid: Option<Vec<u8>>,

    // Received address verification token.
    // token: Option<Vec<u8>>,

    // Error code to be sent to the peer in CONNECTION_CLOSE.
    // error: Option<u64>,

    // Error code to be sent to the peer in APPLICATION_CLOSE.
    // app_error: Option<u64>,

    // Error reason to be sent to the peer in APPLICATION_CLOSE.
    // app_reason: Vec<u8>,

    // Received path challenge.
    // challenge: Option<Vec<u8>>,

    // Idle timeout expiration time.
    // To be confirmed
    // idle_timer: Option<time::Instant>,

    // Draining timeout expiration time.
    // To be confirmed
    // draining_timer: Option<time::Instant>,

    // Whether this is a server-side connection.
    // is_server: bool,

    // Whether the initial secrets have been derived.
    // derived_initial_secrets: bool,

    // Whether a version negotiation packet has already been received. Only
    // relevant for client connections.
    // did_version_negotiation: bool,

    // Whether a retry packet has already been received. Only relevant for
    // client connections.
    // did_retry: bool,

    // Whether the peer already updated its connection ID.
    // got_peer_conn_id: bool,

    // Whether the peer's address has been verified.
    // verified_peer_address: bool,

    // Whether the connection handshake has completed.
    // handshake_completed: bool,

    // Whether the connection handshake has been confirmed.
    // handshake_confirmed: bool,

    // Whether an ACK-eliciting packet has been sent since last receiving a packet.
    // ack_eliciting_sent: bool,

    // Whether the connection is closed.
    // closed: bool,
}

impl Path {
    pub fn new(scid: &[u8], dcid: &[u8], config: &mut Config) -> Path {
        Path {
            scid: scid.to_vec(),
            dcid: dcid.to_vec(),
            pkts_num_with_seq: HashMap::new(),
            last_block: 5,
            previous_unsend: 0,
            previous_bw: 0.0,
            previous_rtt: 0,
            // recovery: recovery::Recovery::new(),
            recovery: recovery::Recovery::new(&config),
            // TODO: set all the parameters
        }
    }

    // TODO: create the Default func for Path
    pub fn get_scid(& self) -> &[u8] {
        &self.scid
    }
    pub fn get_dcid(& self) -> &[u8] {
        &self.dcid
    }
    pub fn set_scid(&mut self, scid: &[u8]) -> () {
        self.scid = scid.to_vec();
    }
    pub fn set_dcid(&mut self, dcid: &[u8]) -> () {
        self.dcid = dcid.to_vec();
    }
}
