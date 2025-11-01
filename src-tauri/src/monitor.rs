use std::{
    collections::HashMap,
    sync::{Mutex, atomic::{AtomicBool, Ordering}},
    thread,
    time::Duration,
};
use once_cell::sync::Lazy;
use serde::Serialize;
use sysinfo::System;

use crate::files;

#[derive(Default, Clone, Serialize, Debug)]
pub(crate) struct Data {
    pub(crate) system: HashMap<String, Vec<f32>>,
    pub(crate) app: HashMap<String, Vec<f32>>,
    pub(crate) general: HashMap<String, f32>,
}

static RUN_MONITOR: AtomicBool = AtomicBool::new(true);
static PERFORMANCE: Lazy<Mutex<Data>> = Lazy::new(|| Mutex::new(Data::default()));

fn run_loop() {
    let mut sys = System::new_all();

    loop {
        if !RUN_MONITOR.load(Ordering::SeqCst) {
            break;
        }

        sys.refresh_all();

        let total_mem: u64 = sys.total_memory() / 1024 / 1024;
        let used_mem: u64 = sys.used_memory() / 1024 / 1024;
        let avg_cpu: f32 = sys.cpus().iter().map(|c| c.cpu_usage()).sum::<f32>() / sys.cpus().len() as f32;

        if let Ok(pid) = sysinfo::get_current_pid() {
            if let Some(proc) = sys.process(pid) {
                let app_cpu: f32 = proc.cpu_usage();
                let app_mem: u64 = proc.memory() / 1024 / 1024;

                let mut per = PERFORMANCE.lock().unwrap();

                per.system.entry("cpu".into()).or_default().push((avg_cpu * 100.0).round() / 100.0);
                per.system.entry("mem".into()).or_default().push(used_mem as f32);

                per.app.entry("cpu".into()).or_default().push((app_cpu * 100.0).round() / 100.0);
                per.app.entry("mem".into()).or_default().push(app_mem as f32);

                per.general.insert("mem".into(), total_mem as f32);

                for vec in per.system.values_mut() {
                    while vec.len() > 10 {
                        vec.remove(0);
                    }
                }

                for vec in per.app.values_mut() {
                    while vec.len() > 10 {
                        vec.remove(0);
                    }
                }
            }
        }

        thread::sleep(Duration::from_secs(5));
    }
}

pub(crate) fn get_data() -> Data {
    PERFORMANCE.lock().unwrap().clone()
}

pub(crate) fn clear_data() {
    let mut per = PERFORMANCE.lock().unwrap();
    per.system.clear();
    per.app.clear();
    per.general.clear();
}

pub(crate) fn start() {
    if files::get_settings().monitor == true {
        if let Err(e) = thread::Builder::new()
            .name("performence-monitor".to_string())
            .spawn(|| run_loop()) {
            eprintln!("Failed to spawn performence monitor thread: {}", e);
        }
    }
}

pub(crate) fn change_bool(new_value: bool) {
    let old_val: bool = RUN_MONITOR.load(Ordering::SeqCst);

    RUN_MONITOR.store(new_value, Ordering::SeqCst);

    if old_val == false && new_value == true {
        start();
    }
}