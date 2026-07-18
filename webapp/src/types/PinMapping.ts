export interface Nrf24 {
    miso: number;
    mosi: number;
    clk: number;
    irq: number;
    en: number;
    cs: number;
}

export interface Cmt2300 {
    clk: number;
    cs: number;
    fcs: number;
    sdio: number;
    gpio2: number;
    gpio3: number;
}

export interface Ethernet {
    enabled: boolean;
    phy_addr: number;
    power: number;
    mdc: number;
    mdio: number;
    type: number;
    clk_mode: number;
}

export interface Can {
    sck: number;
    mosi: number;
    miso: number;
    cs: number;
    int: number;
}

export interface Display {
    type: number;
    data: number;
    clk: number;
    cs: number;
    reset: number;
}

export interface Links {
    name: string;
    url: string;
}

export interface Device {
    name: string;
    links: Array<Links>;
    nrf24: Nrf24;
    cmt: Cmt2300;
    can: Can;
    eth: Ethernet;
    display: Display;
}

export type PinMapping = Array<Device>;
