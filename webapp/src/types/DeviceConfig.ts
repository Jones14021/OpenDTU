import type { Device } from './PinMapping';

export interface Display {
    rotation: number;
    power_safe: boolean;
    screensaver: boolean;
    contrast: number;
    locale: string;
    diagramduration: number;
    diagrammode: number;
}

export interface Led {
    brightness: number;
}

export interface Meanwell {
    npb450_can_address: string;
    npb450_target_power_min: number;
    npb450_target_power_max: number;
}

export interface DeviceConfig {
    curPin: Device;
    display: Display;
    led: Array<Led>;
    meanwell: Meanwell;
}
