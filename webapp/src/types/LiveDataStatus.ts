export interface ValueObject {
    v: number; // value
    u: string; // unit
    d: number; // digits
    max: number;
}

export interface InverterStatistics {
    name: ValueObject;
    Power?: ValueObject;
    Voltage?: ValueObject;
    Current?: ValueObject;
    'Power DC'?: ValueObject;
    YieldDay?: ValueObject;
    YieldTotal?: ValueObject;
    Frequency?: ValueObject;
    Temperature?: ValueObject;
    PowerFactor?: ValueObject;
    ReactivePower?: ValueObject;
    Efficiency?: ValueObject;
    Irradiation?: ValueObject;
}

export interface RadioStatistics {
    tx_request: number;
    tx_re_request: number;
    rx_success: number;
    rx_fail_nothing: number;
    rx_fail_partial: number;
    rx_fail_corrupt: number;
    rssi: number;
}

export interface Inverter {
    serial: string;
    name: string;
    order: number;
    data_age: number;
    data_age_ms: number;
    poll_enabled: boolean;
    reachable: boolean;
    producing: boolean;
    limit_relative: number;
    limit_absolute: number;
    events: number;
    AC: InverterStatistics[];
    DC: InverterStatistics[];
    INV: InverterStatistics[];
    radio_stats: RadioStatistics;
}

export interface Total {
    Power: ValueObject;
    YieldDay: ValueObject;
    YieldTotal: ValueObject;
}

export interface Hints {
    time_sync: boolean;
    default_password: boolean;
    radio_problem: boolean;
    pin_mapping_issue: boolean;
}

export interface ChargerNpb450Status {
    init_state?: string;
    control_enabled?: boolean;
    target_w?: number;
    target_iout_a?: number;
    target_vout_v?: number;
    iout_actual_a?: number;
    iout_actual_seen?: boolean;
    psu_mode_ok?: boolean;
    eeprom_lock_ok?: boolean;
    address?: number;
    validation_seen?: boolean;
    commissioning_pending?: boolean;
    commissioning_requested?: boolean;
    commissioning_next_poll_ms?: number;
    system_status_word?: number;
    system_config_word?: number;
}

export interface ChargerMetricsStatus {
    output_seen?: boolean;
    output_voltage_v?: number;
    output_current_a?: number;
    output_power_w?: number;
    temperature_seen?: boolean;
    temperature_c?: number;
    state_word?: number;
    alarm_word?: number;
}

export interface BatteryMetricsStatus {
    seen?: boolean;
    voltage_v?: number;
    current_a?: number;
    power_w?: number;
}

export interface Mcp2515Pinout {
    sck?: number;
    mosi?: number;
    miso?: number;
    cs?: number;
    int?: number;
}

export interface Mcp2515Status {
    configured?: boolean;
    enabled?: boolean;
    responding?: boolean;
    mode_normal?: boolean;
    active?: boolean;
    rx_seen?: boolean;
    rx_age_ms?: number;
    tx_queue_depth?: number;
    pinout?: Mcp2515Pinout;
}

export interface CanLogEntry {
    timestamp_ms: number;
    id: number;
    ext: boolean;
    rtr: boolean;
    dlc: number;
    data: number[];
    meaning: string;
}

export interface ChargerStatus {
    configured?: boolean;
    enabled: boolean;
    data_age_ms: number;
    mcp2515?: Mcp2515Status;
    npb450: ChargerNpb450Status;
    charger: ChargerMetricsStatus;
    battery: BatteryMetricsStatus;
    can_log?: CanLogEntry[];
}

export interface LiveData {
    inverters: Inverter[];
    total: Total;
    hints: Hints;
    charger: ChargerStatus;
}
