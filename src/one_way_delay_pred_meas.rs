pub struct OneWayDelayPredMeas {
    initial_path_owd: f64,
    subseq_path_owd: f64,
    owd_diff_stdev: f64,
    ewma_alpha: f64,
    owd_diffs: Vec<f64>,
    owds_diffs_len: usize,
}

impl Default for OneWayDelayPredMeas {
    fn default() -> OneWayDelayPredMeas {
        OneWayDelayPredMeas {
            initial_path_owd: 5 as f64,
            subseq_path_owd: 5 as f64,
            owd_diff_stdev: 0 as f64,
            ewma_alpha: 0.25, //a default parameter value
            owd_diffs: Vec::new(),
            owds_diffs_len: 20,
        }
    }
}

impl OneWayDelayPredMeas {
    // pub fn is_small_owd_path(&self, path: u8) -> bool {
    //     match path {
    //         0 => {
    //             if self.initial_path_owd <= self.subseq_path_owd {
    //                 return true;
    //             }
    //             false
    //         }

    //         1 => {
    //             if self.subseq_path_owd < self.initial_path_owd {
    //                 return true;
    //             }
    //             false
    //         }

    //         _ => panic!("Path ID is wrong!")
    //     }
    // }

    // pub fn get_owd_diff_pred(&self) -> f64 {
    //     let owd_diff = self.initial_path_owd - self.subseq_path_owd;

    //     owd_diff.abs()
    // }

    // pub fn get_owd_diff_stdev(&self) -> f64 {
    //     self.owd_diff_stdev
    // }

    // OWD: ms
    pub fn get_owd_pred(&self, path: usize) -> f64 {
        match path {
            0 => self.initial_path_owd,
            1 => self.subseq_path_owd,
            _ => panic!("Path ID is wrong!"),
        }
    }

    // using EWMA to update
    pub fn update_owd(&mut self, owd: f64, path: u8) {
        match path {
            0 => {
                self.initial_path_owd = owd * self.ewma_alpha
                    + self.initial_path_owd * (1.0 - self.ewma_alpha);
            }

            1 => {
                self.subseq_path_owd = owd * self.ewma_alpha
                    + self.subseq_path_owd * (1.0 - self.ewma_alpha);
            }

            _ => panic!("Path ID is wrong!"),
        }

        let owd_diffs_num = self.owd_diffs.len();
        let owd_diff = (self.initial_path_owd - self.subseq_path_owd).abs();

        if owd_diffs_num < self.owds_diffs_len {
            self.owd_diffs.push(owd_diff);
        } else {
            let mut counter: usize = 0;
            while counter + 1 < owd_diffs_num {
                self.owd_diffs[counter] = self.owd_diffs[counter + 1];
                counter += 1;
            }
            self.owd_diffs[counter] = owd_diff;
            self.owd_diff_stdev = self.calculate_stdev();
        }
    }

    fn calculate_stdev(&mut self) -> f64 {
        let mut sum = 0 as f64;
        let mut stdev = 0 as f64;
        let owd_diffs_num = self.owd_diffs.len();
        let mean: f64;
        let divisor: usize;

        for e in self.owd_diffs.iter_mut() {
            sum += *e;
        }
        mean = sum / owd_diffs_num as f64;
        for e in self.owd_diffs.iter_mut() {
            stdev += (*e - mean).powi(2);
        }

        if owd_diffs_num <= 20 {
            divisor = owd_diffs_num - 1;
        } else {
            divisor = owd_diffs_num;
        }

        stdev / divisor as f64
    }
}
