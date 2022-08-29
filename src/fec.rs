use std::collections::BTreeMap;
use std::collections::HashSet;

/// FEC status
#[derive(Default, PartialEq, Clone, Copy, Debug)]
pub struct FecStatus {
    pub m: u8,
    pub n: u8,
    pub group_id: u32,
    pub index: u8,
}

// impl std::fmt::Debug for FecStatus {
//     fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
//         write!(
//             f,
//             "group id={:?}, m={:?}, n={:?}, index ={:?}",
//             self.group_id, self.m, self.n, self.index
//         )?;
//         Ok(())
//     }
// }

#[derive(Default)]
pub struct FecPacketNumbers {
    pub m: u8,
    pub n: u8,
    // <index, pn>
    pub packet_number_sent: BTreeMap<u8, u64>,
    pub packet_number_acked: HashSet<u64>,
    pub packet_number_lost: HashSet<u64>,
}

impl FecPacketNumbers {
    pub fn new(m: u8, n: u8) -> FecPacketNumbers {
        FecPacketNumbers {
            m,
            n,
            ..Default::default()
        }
    }

    pub fn packet_sent(&mut self, index: u8, pn: u64) {
        self.packet_number_sent.insert(index, pn);
    }

    pub fn packet_acked(&mut self, pn: u64) {
        self.packet_number_acked.insert(pn);
    }

    pub fn packet_lost(&mut self, pn: u64) {
        self.packet_number_lost.insert(pn);
    }

    pub fn get_delta(&self) -> Option<i8> {
        if self.packet_number_lost.len() + self.packet_number_acked.len()
            < self.m as usize + self.n as usize
        {
            return None;
        } else {
            return Some(self.packet_number_acked.len() as i8 - self.m as i8);
        }
    }

    pub fn get_recovered_pns(&self) -> Option<Vec<u64>> {
        if self.packet_number_acked.len() >= self.m as usize {
            let mut packet_numer_recovered: Vec<u64> = Vec::new();
            for pn in self.packet_number_sent.values() {
                if !self.packet_number_acked.contains(pn) {
                    packet_numer_recovered.push(*pn);
                }
            }
            return Some(packet_numer_recovered);
        } else {
            return None;
        }
    }
}

#[derive(Default, Clone)]
pub struct FecFrame {
    pub info: FecStatus,
    pub data: Vec<u8>,
}

impl std::fmt::Debug for FecFrame {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(
            f,
            "FecFrame status: {:?}, data length: {}",
            self.info,
            self.data.len()
        )?;
        Ok(())
    }
}

pub trait FecAlgorithm {
    fn encode(&self, v: &mut Vec<Vec<u8>>) -> Result<(), &'static str>;
    fn reconstruct_data(
        &self, shards: &mut Vec<Option<Vec<u8>>>,
    ) -> Result<(), &'static str>;
    fn verify(&self, shards: &Vec<Vec<u8>>) -> Result<bool, &'static str>;
}

/// a fake and empty Fec algorithm
#[cfg(not(feature = "fec"))]
#[allow(dead_code)]
pub struct RSFec {
    m: usize,
    n: usize,
}

#[cfg(not(feature = "fec"))]
impl RSFec {
    pub fn new(m: usize, n: usize) -> Result<RSFec, &'static str> {
        return Ok(RSFec { m, n });
    }
}

#[cfg(not(feature = "fec"))]
impl FecAlgorithm for RSFec {
    fn encode(&self, _v: &mut Vec<Vec<u8>>) -> Result<(), &'static str> {
        return Ok(());
    }
    fn reconstruct_data(
        &self, _shards: &mut Vec<Option<Vec<u8>>>,
    ) -> Result<(), &'static str> {
        return Ok(());
    }
    fn verify(&self, _shards: &Vec<Vec<u8>>) -> Result<bool, &'static str> {
        return Ok(true);
    }
}

#[cfg(feature = "fec")]
use reed_solomon_erasure::galois_8::ReedSolomon;
#[cfg(feature = "fec")]
pub struct RSFec {
    r: Box<ReedSolomon>,
}
#[cfg(feature = "fec")]
impl RSFec {
    pub fn new(m: usize, n: usize) -> Result<Self, &'static str> {
        Ok(RSFec {
            r: Box::new(ReedSolomon::new(m, n).unwrap()),
        })
    }
}
#[cfg(feature = "fec")]
impl FecAlgorithm for RSFec {
    fn encode(
        &self, filled_shards: &mut Vec<Vec<u8>>,
    ) -> Result<(), &'static str> {
        match self.r.encode(filled_shards) {
            Ok(()) => return Ok(()),
            Err(_) => return Err("error in RS fec encode"),
        };
    }
    fn reconstruct_data(
        &self, shards: &mut Vec<Option<Vec<u8>>>,
    ) -> Result<(), &'static str> {
        match self.r.reconstruct_data(shards) {
            Ok(()) => return Ok(()),
            Err(_) => return Err("error in RS fec reconstruct."),
        };
    }
    fn verify(&self, shards: &Vec<Vec<u8>>) -> Result<bool, &'static str> {
        match self.r.verify(&shards) {
            Ok(b) => return Ok(b),
            Err(_) => return Err("error in RS fec verify"),
        };
    }
}

/// FEC group using shards by shards.
pub struct FecGroup {
    pub info: FecStatus,
    pub shard_size: usize,
    pub r: Box<dyn FecAlgorithm>,
    pub next_index: u8,
    pub shards: Vec<Option<Vec<u8>>>,
    pub restored: bool,
}

impl FecGroup {
    pub fn new(info: FecStatus, shard_size: usize) -> FecGroup {
        FecGroup {
            info,
            shard_size,
            r: Box::new(RSFec::new(info.m as usize, info.n as usize).unwrap()),
            shards: vec![None; info.m as usize + info.n as usize],
            next_index: 0,
            restored: false,
        }
    }

    pub fn feed_fec_frame(&mut self, frame: &FecFrame) {
        assert_eq!(frame.info.m, self.info.m);
        assert_eq!(frame.info.n, self.info.n);
        assert_eq!(frame.info.group_id, self.info.group_id);
        assert_eq!(frame.data.len(), self.shard_size);
        self.info.index = frame.info.index; // Recode last index of feeded frame
        self.shards[frame.info.index as usize] = Some(frame.data.clone());
    }

    pub fn full(&self) -> bool {
        self.next_index >= self.info.m
    }

    pub fn encode(&mut self) -> Vec<FecFrame> {
        // Frist we fill the remaining shards
        // let begin = time::get_time();
        let mut filled_shards: Vec<Vec<u8>> = self
            .shards
            .clone()
            .into_iter()
            .map(|x| x.unwrap_or(vec![0u8; self.shard_size]))
            .collect();
        self.r.encode(&mut filled_shards).unwrap();
        // Then we fill back the redundancy shards
        let mut r: Vec<FecFrame> = Vec::new();
        for index in self.info.m..self.info.m + self.info.n {
            r.push(FecFrame {
                info: FecStatus { index, ..self.info },
                data: filled_shards[index as usize].clone(),
            });
        }
        // let end = time::get_time();
        // debug!("encode: {}", end - begin);
        return r;
    }

    #[allow(dead_code)]
    pub fn verify(&mut self) {
        let shards: Vec<Vec<u8>> = self
            .shards
            .clone()
            .into_iter()
            .map(|x| x.unwrap_or(vec![0u8; self.shard_size]))
            .collect();
        self.r.verify(&shards).unwrap();
    }

    pub fn reconstruct(&mut self) -> Option<Vec<Vec<u8>>> {
        // First we take a note of missing data.
        // let begin = time::get_time();
        let mut empty = Vec::new();
        for (i, shard) in self.shards.iter().enumerate() {
            if shard.is_none() && i < self.info.m as usize {
                // Only mark data shards
                empty.push(i);
            }
        }
        if empty.is_empty() {
            return None;
        } else {
            match self.r.reconstruct_data(&mut self.shards) {
                Ok(()) => {
                    let mut r: Vec<Vec<u8>> = Vec::new();
                    for i in empty {
                        r.push(self.shards[i].as_ref().unwrap().clone()); // TO understand,
                                                                          // why
                                                                          // do we
                                                                          // need
                                                                          // as_ref
                    }
                    // let end = time::get_time();
                    // debug!("reconstruct ok: {}", end - begin);
                    return Some(r);
                },
                Err(_) => {
                    // let end = time::get_time();
                    // debug!("reconstruct fail: {}", end - begin);
                    return None;
                },
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn encode() {
        let mut fec_frames: Vec<FecFrame> = Vec::new();
        for index in 0..3 {
            fec_frames.push(FecFrame {
                info: FecStatus {
                    m: 3,
                    n: 2,
                    group_id: 100,
                    index,
                },
                data: vec![4 * index; 4],
            })
        }

        let mut sender_fec = FecGroup::new(fec_frames[0].info, 4);
        sender_fec.feed_fec_frame(&fec_frames[0]);
        sender_fec.feed_fec_frame(&fec_frames[1]);
        // sender_fec.feed_fec_frame(&fec_frames[2]);
        let r = sender_fec.encode();
        sender_fec.verify();
        let mut receiver_fec = FecGroup::new(fec_frames[0].info, 4);
        receiver_fec.feed_fec_frame(&fec_frames[0]);
        // receiver_fec.feed_fec_frame(&fec_frames[1]);
        receiver_fec.feed_fec_frame(&r[0]);
        receiver_fec.feed_fec_frame(&r[1]);

        let rc = receiver_fec.reconstruct().unwrap();
        assert_eq!(rc[0], fec_frames[1].data);
    }
}
