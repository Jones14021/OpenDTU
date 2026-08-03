<template>
    <BasePage
        :title="$t('home.LiveData')"
        :isLoading="dataLoading"
        :isWideScreen="true"
        :showWebSocket="true"
        :isWebsocketConnected="isWebsocketConnected"
        @reload="reloadData"
    >
        <HintView :hints="liveData.hints" />
        <InverterTotalInfo :totalData="liveData.total" /><br />
        <div class="row gy-3 mb-2" v-if="liveData.charger">
            <div class="col-12">
                <div class="card border-info h-100">
                    <div class="card-header d-flex justify-content-between flex-wrap gap-2 align-items-center">
                        <div class="fw-semibold">{{ $t('home.ChargerStatus') }}</div>
                        <div class="d-flex gap-2 flex-wrap align-items-center">
                            <span class="badge text-bg-secondary" v-if="!liveData.charger.configured">
                                {{ $t('home.McpNotConfigured') }}
                            </span>
                            <span class="badge text-bg-secondary" v-else-if="!liveData.charger.enabled">
                                {{ $t('home.Disabled') }}
                            </span>
                            <template v-else>
                                <span class="badge" :class="chargerInitClass(liveData.charger.npb450.init_state)">
                                    {{ $t('home.NpbInit_' + (liveData.charger.npb450.init_state || 'disabled')) }}
                                </span>
                                <span
                                    class="badge"
                                    :class="
                                        liveData.charger.npb450.control_enabled
                                            ? 'text-bg-success'
                                            : 'text-bg-secondary'
                                    "
                                >
                                    {{
                                        liveData.charger.npb450.control_enabled
                                            ? $t('home.ControlEnabled')
                                            : $t('home.ControlDisabled')
                                    }}
                                </span>
                                <span
                                    class="badge"
                                    :class="liveData.charger.npb450.charge_enabled ? 'text-bg-success' : 'text-bg-secondary'"
                                >
                                    {{
                                        liveData.charger.npb450.charge_enabled
                                            ? $t('home.ChargeEnabled')
                                            : $t('home.ChargeDisabled')
                                    }}
                                </span>
                            </template>
                            <DataAgeDisplay v-if="liveData.charger.data_age_ms >= 0" :data-age-ms="liveData.charger.data_age_ms" />
                            <span class="badge text-bg-warning" v-else>{{ $t('home.WaitingForChargerData') }}</span>
                            <button
                                type="button"
                                class="btn btn-sm btn-outline-primary"
                                :disabled="!liveData.charger.mcp2515?.configured || !liveData.charger.enabled || !isLogged"
                                @click="openCanDialog"
                            >
                                {{ $t('home.CanDebug') }}
                            </button>
                            <button
                                type="button"
                                class="btn btn-sm btn-outline-warning"
                                :disabled="!liveData.charger.mcp2515?.configured || !liveData.charger.enabled || !isLogged"
                                @click="openPsuCommissionDialog"
                            >
                                {{ $t('home.CommissionPsuMode') }}
                            </button>
                        </div>
                    </div>
                    <div class="card-body">
                        <BootstrapAlert
                            :show="liveData.charger.enabled && liveData.charger.npb450.psu_mode_ok === false"
                            variant="warning"
                        >
                            {{ $t('home.PsuModeRequiredWarning') }}
                            <button type="button" class="btn btn-link alert-link p-0 ms-1" @click="openPsuCommissionDialog">
                                {{ $t('home.OpenPsuCommissionDialog') }}
                            </button>
                        </BootstrapAlert>
                        <div class="row g-3">
                            <div class="col-lg-4">
                                <h6 class="text-uppercase text-muted mb-2">{{ $t('home.Npb450Control') }}</h6>
                                <table class="table table-sm table-striped mb-0">
                                    <tbody>
                                        <tr>
                                            <td>{{ $t('home.McpConfigured') }}</td>
                                            <td>{{ liveData.charger.mcp2515?.configured ? $t('base.Yes') : $t('base.No') }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.McpResponding') }}</td>
                                            <td>{{ liveData.charger.mcp2515?.responding ? $t('base.Yes') : $t('base.No') }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.McpModeNormal') }}</td>
                                            <td>{{ liveData.charger.mcp2515?.mode_normal ? $t('base.Yes') : $t('base.No') }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.McpActive') }}</td>
                                            <td>
                                                <span
                                                    class="badge"
                                                    :class="liveData.charger.mcp2515?.active ? 'text-bg-success' : 'text-bg-danger'"
                                                >
                                                    {{
                                                        liveData.charger.mcp2515?.active
                                                            ? $t('home.Active')
                                                            : $t('home.Inactive')
                                                    }}
                                                </span>
                                            </td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.McpRxAge') }}</td>
                                            <td>
                                                <DataAgeDisplay
                                                    v-if="
                                                        liveData.charger.mcp2515 &&
                                                        (liveData.charger.mcp2515.rx_age_ms ?? -1) >= 0
                                                    "
                                                    :data-age-ms="liveData.charger.mcp2515?.rx_age_ms || 0"
                                                />
                                                <span v-else>-</span>
                                            </td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.McpTxQueue') }}</td>
                                            <td>{{ liveData.charger.mcp2515?.tx_queue_depth ?? 0 }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.Address') }}</td>
                                            <td>{{ liveData.charger.npb450.address ?? '-' }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.TargetPower') }}</td>
                                            <td>{{ $n(liveData.charger.npb450.target_w ?? 0, 'decimalOneDigit') }} W</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.TargetVoltage') }}</td>
                                            <td>{{ $n(liveData.charger.npb450.target_vout_v ?? 0, 'decimalTwoDigits') }} V</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.TargetCurrent') }}</td>
                                            <td>{{ $n(liveData.charger.npb450.target_iout_a ?? 0, 'decimalTwoDigits') }} A</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.ActualCurrent') }}</td>
                                            <td>{{ $n(liveData.charger.npb450.iout_actual_a ?? 0, 'decimalTwoDigits') }} A</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.PsuModeOk') }}</td>
                                            <td>{{ liveData.charger.npb450.psu_mode_ok ? $t('base.Yes') : $t('base.No') }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.EepromLockOk') }}</td>
                                            <td>
                                                {{ liveData.charger.npb450.eeprom_lock_ok ? $t('base.Yes') : $t('base.No') }}
                                            </td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.SystemStatusWord') }}</td>
                                            <td>{{ formatWord(liveData.charger.npb450.system_status_word) }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.SystemConfigWord') }}</td>
                                            <td>{{ formatWord(liveData.charger.npb450.system_config_word) }}</td>
                                        </tr>
                                    </tbody>
                                </table>
                            </div>
                            <div class="col-lg-4">
                                <h6 class="text-uppercase text-muted mb-2">{{ $t('home.Npb450Output') }}</h6>
                                <table class="table table-sm table-striped mb-0">
                                    <tbody>
                                        <tr>
                                            <td>{{ $t('home.OutputVoltage') }}</td>
                                            <td>{{ $n(liveData.charger.npb450.vout_actual_v ?? 0, 'decimalTwoDigits') }} V</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.OutputCurrent') }}</td>
                                            <td>{{ $n(liveData.charger.npb450.iout_actual_a ?? 0, 'decimalTwoDigits') }} A</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.OutputPower') }}</td>
                                            <td>{{ $n(liveData.charger.npb450.output_power_w ?? 0, 'decimalOneDigit') }} W</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.OutputEnergy') }}</td>
                                            <td>{{ liveData.charger.npb450.output_energy_kwh?.toFixed(3) ?? '-' }} kWh</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.OutputEfficiency') }}</td>
                                            <td>{{ liveData.charger.npb450.efficiency_percent?.toFixed(1) ?? '-' }} %</td>
                                        </tr>
                                    </tbody>
                                </table>
                            </div>
                            <div class="col-lg-4">
                                <h6 class="text-uppercase text-muted mb-2">{{ $t('home.CanInterface') }}</h6>
                                <table class="table table-sm table-striped mb-0">
                                    <tbody>
                                        <tr>
                                            <td>{{ $t('home.PinSck') }}</td>
                                            <td>{{ liveData.charger.mcp2515?.pinout?.sck ?? '-' }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.PinMosi') }}</td>
                                            <td>{{ liveData.charger.mcp2515?.pinout?.mosi ?? '-' }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.PinMiso') }}</td>
                                            <td>{{ liveData.charger.mcp2515?.pinout?.miso ?? '-' }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.PinCs') }}</td>
                                            <td>{{ liveData.charger.mcp2515?.pinout?.cs ?? '-' }}</td>
                                        </tr>
                                        <tr>
                                            <td>{{ $t('home.PinInt') }}</td>
                                            <td>{{ liveData.charger.mcp2515?.pinout?.int ?? '-' }}</td>
                                        </tr>
                                    </tbody>
                                </table>
                            </div>
                            <div class="col-12">
                                <h6 class="text-uppercase text-muted mb-2">{{ $t('home.CanRxLog') }}</h6>
                                <div class="table-responsive">
                                    <table class="table table-sm table-striped mb-0">
                                        <thead>
                                            <tr>
                                                <th>{{ $t('home.TimeMs') }}</th>
                                                <th>{{ $t('home.FrameId') }}</th>
                                                <th>{{ $t('home.Flags') }}</th>
                                                <th>{{ $t('home.Data') }}</th>
                                                <th>{{ $t('home.Interpretation') }}</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            <tr v-if="!chargerCanLog.length">
                                                <td colspan="5" class="text-muted">{{ $t('home.NoCanLog') }}</td>
                                            </tr>
                                            <tr v-for="(entry, index) in chargerCanLog" :key="'canlog-' + index">
                                                <td>{{ entry.timestamp_ms }}</td>
                                                <td>{{ formatCanId(entry.id, entry.ext) }}</td>
                                                <td>{{ formatCanFlags(entry) }}</td>
                                                <td>{{ formatCanData(entry.data, entry.dlc) }}</td>
                                                <td>{{ entry.meaning }}</td>
                                            </tr>
                                        </tbody>
                                    </table>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        <div class="row gy-3">
            <div class="col-sm-3 col-md-2" :style="[inverterData.length == 1 ? { display: 'none' } : {}]">
                <div
                    class="nav nav-pills row-cols-sm-1 gap-3"
                    id="v-pills-tab"
                    role="tablist"
                    aria-orientation="vertical"
                >
                    <button
                        v-for="inverter in inverterData"
                        :key="inverter.serial"
                        class="nav-link border border-primary text-break"
                        :id="'v-pills-' + inverter.serial + '-tab'"
                        data-bs-toggle="pill"
                        :data-bs-target="'#v-pills-' + inverter.serial"
                        type="button"
                        role="tab"
                        aria-controls="'v-pills-' + inverter.serial"
                        aria-selected="true"
                    >
                        <div class="d-flex align-items-center">
                            <div class="me-2">
                                <span
                                    v-if="inverter.AC"
                                    class="badge"
                                    :class="{
                                        'text-bg-secondary': !inverter.poll_enabled,
                                        'text-bg-danger': inverter.poll_enabled && !inverter.reachable,
                                        'text-bg-warning':
                                            inverter.poll_enabled && inverter.reachable && !inverter.producing,
                                        'text-bg-success':
                                            inverter.poll_enabled && inverter.reachable && inverter.producing,
                                    }"
                                >
                                    {{ $n(inverter.AC[0]?.Power?.v || 0, 'decimalNoDigits') }}
                                    {{ inverter.AC[0]?.Power?.u }}
                                </span>
                                <span v-else class="badge text-bg-light">-</span>
                            </div>
                            <div class="ms-auto me-auto">
                                {{ inverter.name }}
                            </div>
                        </div>
                    </button>
                </div>
            </div>

            <div
                class="tab-content"
                id="v-pills-tabContent"
                :class="{
                    'col-sm-9 col-md-10': inverterData.length > 1,
                    'col-sm-12 col-md-12': inverterData.length == 1,
                }"
            >
                <div
                    v-for="inverter in inverterData"
                    :key="inverter.serial"
                    class="tab-pane fade show"
                    :id="'v-pills-' + inverter.serial"
                    role="tabpanel"
                    :aria-labelledby="'v-pills-' + inverter.serial + '-tab'"
                    tabindex="0"
                >
                    <div class="card">
                        <div
                            class="card-header d-flex justify-content-between align-items-center"
                            :class="{
                                'text-bg-tertiary': !inverter.poll_enabled,
                                'text-bg-danger': inverter.poll_enabled && !inverter.reachable,
                                'text-bg-warning': inverter.poll_enabled && inverter.reachable && !inverter.producing,
                                'text-bg-success': inverter.poll_enabled && inverter.reachable && inverter.producing,
                            }"
                        >
                            <div class="p-1 flex-grow-1">
                                <div class="d-flex flex-wrap">
                                    <div style="padding-right: 2em">
                                        {{ inverter.name }}
                                    </div>
                                    <div style="padding-right: 2em">
                                        {{ $t('home.SerialNumber') }}{{ inverter.serial }}
                                    </div>
                                    <div style="padding-right: 2em">
                                        {{ $t('home.CurrentLimit') }}:
                                        <template v-if="inverter.limit_absolute > -1">
                                            {{ $n(inverter.limit_absolute, 'decimalNoDigits') }} W | </template
                                        >{{ $n(inverter.limit_relative / 100, 'percentOneDigit') }}
                                    </div>
                                    <div style="padding-right: 2em">
                                        <DataAgeDisplay :data-age-ms="inverter.data_age_ms" />
                                    </div>
                                </div>
                            </div>
                            <div class="btn-toolbar p-2" role="toolbar">
                                <div class="btn-group me-2" role="group">
                                    <button
                                        :disabled="!isLogged"
                                        type="button"
                                        class="btn btn-sm btn-danger"
                                        @click="onShowLimitSettings(inverter.serial)"
                                        v-tooltip
                                        :title="$t('home.ShowSetInverterLimit')"
                                    >
                                        <BIconSpeedometer style="font-size: 24px" />
                                    </button>
                                </div>

                                <div class="btn-group me-2" role="group">
                                    <button
                                        :disabled="!isLogged"
                                        type="button"
                                        class="btn btn-sm btn-danger"
                                        @click="onShowPowerSettings(inverter.serial)"
                                        v-tooltip
                                        :title="$t('home.TurnOnOff')"
                                    >
                                        <BIconPower style="font-size: 24px" />
                                    </button>
                                </div>

                                <div class="btn-group me-2" role="group">
                                    <button
                                        type="button"
                                        class="btn btn-sm btn-info"
                                        @click="onShowDevInfo(inverter.serial)"
                                        v-tooltip
                                        :title="$t('home.ShowInverterInfo')"
                                    >
                                        <BIconCpu style="font-size: 24px" />
                                    </button>
                                </div>

                                <div class="btn-group me-2" role="group">
                                    <button
                                        type="button"
                                        class="btn btn-sm btn-info"
                                        @click="onShowGridProfile(inverter.serial)"
                                        v-tooltip
                                        :title="$t('home.ShowGridProfile')"
                                    >
                                        <BIconOutlet style="font-size: 24px" />
                                    </button>
                                </div>

                                <div class="btn-group" role="group">
                                    <button
                                        v-if="inverter.events >= 0"
                                        type="button"
                                        class="btn btn-sm btn-secondary position-relative"
                                        @click="onShowEventlog(inverter.serial)"
                                        v-tooltip
                                        :title="$t('home.ShowEventlog')"
                                    >
                                        <BIconJournalText style="font-size: 24px" />
                                        <span
                                            class="position-absolute top-0 start-100 translate-middle badge rounded-pill text-bg-danger"
                                        >
                                            {{ inverter.events }}
                                            <span class="visually-hidden">{{ $t('home.UnreadMessages') }}</span>
                                        </span>
                                    </button>
                                </div>
                            </div>
                        </div>
                        <div class="card-body">
                            <div class="row flex-row-reverse flex-wrap-reverse g-3">
                                <template
                                    v-for="chanType in [
                                        { obj: inverter.INV, name: 'INV' },
                                        { obj: inverter.AC, name: 'AC' },
                                        { obj: inverter.DC, name: 'DC' },
                                    ].reverse()"
                                >
                                    <template v-if="chanType.obj != null">
                                        <template
                                            v-for="channel in Object.keys(chanType.obj)
                                                .sort()
                                                .reverse()
                                                .map((x) => +x)"
                                            :key="channel"
                                        >
                                            <template
                                                v-if="
                                                    chanType.name != 'DC' ||
                                                    (chanType.name == 'DC' && getSumIrridiation(inverter) == 0) ||
                                                    (chanType.name == 'DC' &&
                                                        getSumIrridiation(inverter) > 0 &&
                                                        chanType.obj[channel]?.Irradiation?.max) ||
                                                    0 > 0
                                                "
                                            >
                                                <div class="col" v-if="chanType.obj[channel]">
                                                    <InverterChannelInfo
                                                        :channelData="chanType.obj[channel]"
                                                        :channelType="chanType.name"
                                                        :channelNumber="channel"
                                                    />
                                                </div>
                                            </template>
                                        </template>
                                    </template>
                                </template>
                            </div>

                            <BootstrapAlert class="m-3" :show="!inverter.hasOwnProperty('INV')">
                                <div class="d-flex justify-content-center align-items-center">
                                    <div class="spinner-border m-1" role="status">
                                        <span class="visually-hidden">{{ $t('home.LoadingInverter') }}</span>
                                    </div>
                                    <span>{{ $t('home.LoadingInverter') }}</span>
                                </div>
                            </BootstrapAlert>

                            <div class="accordion mt-5" id="accordionRadioStats">
                                <div class="accordion-item accordion-table">
                                    <h2 class="accordion-header">
                                        <button
                                            class="accordion-button collapsed"
                                            type="button"
                                            data-bs-toggle="collapse"
                                            data-bs-target="#collapseStats"
                                            aria-expanded="true"
                                            aria-controls="collapseStats"
                                        >
                                            <BIconBroadcast />&nbsp;{{ $t('home.RadioStats') }}
                                        </button>
                                    </h2>
                                    <div
                                        id="collapseStats"
                                        class="accordion-collapse collapse"
                                        data-bs-parent="#accordionRadioStats"
                                    >
                                        <div class="accordion-body">
                                            <table class="table table-striped table-hover">
                                                <tbody>
                                                    <tr>
                                                        <td>{{ $t('home.TxRequest') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.tx_request) }}</td>
                                                        <td></td>
                                                    </tr>
                                                    <tr>
                                                        <td>{{ $t('home.RxSuccess') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.rx_success) }}</td>
                                                        <td>
                                                            {{
                                                                ratio(
                                                                    inverter.radio_stats.rx_success,
                                                                    inverter.radio_stats.tx_request
                                                                )
                                                            }}
                                                        </td>
                                                    </tr>
                                                    <tr>
                                                        <td>{{ $t('home.RxFailNothing') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.rx_fail_nothing) }}</td>
                                                        <td>
                                                            {{
                                                                ratio(
                                                                    inverter.radio_stats.rx_fail_nothing,
                                                                    inverter.radio_stats.tx_request
                                                                )
                                                            }}
                                                        </td>
                                                    </tr>
                                                    <tr>
                                                        <td>{{ $t('home.RxFailPartial') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.rx_fail_partial) }}</td>
                                                        <td>
                                                            {{
                                                                ratio(
                                                                    inverter.radio_stats.rx_fail_partial,
                                                                    inverter.radio_stats.tx_request
                                                                )
                                                            }}
                                                        </td>
                                                    </tr>
                                                    <tr>
                                                        <td>{{ $t('home.RxFailCorrupt') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.rx_fail_corrupt) }}</td>
                                                        <td>
                                                            {{
                                                                ratio(
                                                                    inverter.radio_stats.rx_fail_corrupt,
                                                                    inverter.radio_stats.tx_request
                                                                )
                                                            }}
                                                        </td>
                                                    </tr>
                                                    <tr>
                                                        <td>{{ $t('home.TxReRequest') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.tx_re_request) }}</td>
                                                        <td></td>
                                                    </tr>
                                                    <tr>
                                                        <td>
                                                            {{ $t('home.Rssi') }}
                                                            <BIconInfoCircle v-tooltip :title="$t('home.RssiHint')" />
                                                        </td>
                                                        <td>
                                                            {{ $t('home.dBm', { dbm: $n(inverter.radio_stats.rssi) }) }}
                                                        </td>
                                                        <td></td>
                                                    </tr>
                                                </tbody>
                                            </table>
                                            <div class="d-flex">
                                                <button
                                                    :disabled="!isLogged || performRadioStatsReset"
                                                    type="button"
                                                    class="btn btn-danger ms-auto me-3 mt-3"
                                                    @click="onResetRadioStats(inverter.serial)"
                                                >
                                                    <template v-if="!performRadioStatsReset">
                                                        <BIconArrowCounterclockwise />&nbsp;{{ $t('home.StatsReset') }}
                                                    </template>
                                                    <template v-else>
                                                        <span
                                                            class="spinner-border spinner-border-sm"
                                                            aria-hidden="true"
                                                        ></span>
                                                        <span role="status">&nbsp;{{ $t('home.StatsResetting') }}</span>
                                                    </template>
                                                </button>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </BasePage>

    <ModalDialog modalId="chargerCanDebugView" :title="$t('home.CanDebug')" :loading="chargerCanLoading">
        <BootstrapAlert v-model="showAlertCan" :variant="alertTypeCan">
            {{ alertMessageCan }}
        </BootstrapAlert>

        <div class="row mb-3">
            <label for="canPreset" class="col-sm-3 col-form-label">{{ $t('home.Preset') }}</label>
            <div class="col-sm-9">
                <select id="canPreset" class="form-select" v-model="selectedCanPreset" @change="onApplyCanPreset">
                    <option value="">{{ $t('home.CustomFrame') }}</option>
                    <option v-for="preset in canPresetOptions" :key="preset.id" :value="preset.id">
                        {{ preset.label }}
                    </option>
                </select>
            </div>
        </div>

        <div class="row mb-3">
            <label for="canId" class="col-sm-3 col-form-label">{{ $t('home.FrameId') }}</label>
            <div class="col-sm-9">
                <input id="canId" type="text" class="form-control" v-model="canFrameForm.idHex" />
            </div>
        </div>

        <div class="row mb-3">
            <label for="canDlc" class="col-sm-3 col-form-label">{{ $t('home.Dlc') }}</label>
            <div class="col-sm-9">
                <input id="canDlc" type="number" min="0" max="8" class="form-control" v-model.number="canFrameForm.dlc" />
            </div>
        </div>

        <div class="row mb-3">
            <label for="canData" class="col-sm-3 col-form-label">{{ $t('home.DataBytes') }}</label>
            <div class="col-sm-9">
                <input id="canData" type="text" class="form-control" v-model="canFrameForm.dataHex" />
            </div>
        </div>

        <div class="row mb-3">
            <div class="col-sm-3"></div>
            <div class="col-sm-9 d-flex gap-4">
                <div class="form-check">
                    <input id="canExt" class="form-check-input" type="checkbox" v-model="canFrameForm.ext" />
                    <label class="form-check-label" for="canExt">{{ $t('home.ExtendedFrame') }}</label>
                </div>
                <div class="form-check">
                    <input id="canRtr" class="form-check-input" type="checkbox" v-model="canFrameForm.rtr" />
                    <label class="form-check-label" for="canRtr">{{ $t('home.RemoteRequest') }}</label>
                </div>
            </div>
        </div>

        <div class="d-flex justify-content-end mb-4">
            <button class="btn btn-primary" type="button" @click="onSendCanFrame" :disabled="sendingCanFrame || !isLogged">
                <template v-if="sendingCanFrame">
                    <span class="spinner-border spinner-border-sm" aria-hidden="true"></span>
                    <span role="status">&nbsp;{{ $t('home.Sending') }}</span>
                </template>
                <template v-else>
                    {{ $t('home.SendCanFrame') }}
                </template>
            </button>
        </div>

        <h6 class="text-uppercase text-muted mb-2">{{ $t('home.CanRxLog') }}</h6>
        <div class="table-responsive can-log-dialog-table">
            <table class="table table-sm table-striped mb-0">
                <thead>
                    <tr>
                        <th>{{ $t('home.TimeMs') }}</th>
                        <th>{{ $t('home.FrameId') }}</th>
                        <th>{{ $t('home.Flags') }}</th>
                        <th>{{ $t('home.Data') }}</th>
                        <th>{{ $t('home.Interpretation') }}</th>
                    </tr>
                </thead>
                <tbody>
                    <tr v-if="!chargerCanLog.length">
                        <td colspan="5" class="text-muted">{{ $t('home.NoCanLog') }}</td>
                    </tr>
                    <tr v-for="(entry, index) in chargerCanLog" :key="'canlog-dialog-' + index">
                        <td>{{ entry.timestamp_ms }}</td>
                        <td>{{ formatCanId(entry.id, entry.ext) }}</td>
                        <td>{{ formatCanFlags(entry) }}</td>
                        <td>{{ formatCanData(entry.data, entry.dlc) }}</td>
                        <td>{{ entry.meaning }}</td>
                    </tr>
                </tbody>
            </table>
        </div>
    </ModalDialog>

    <ModalDialog modalId="psuCommissionView" :title="$t('home.CommissionPsuMode')">
        <BootstrapAlert v-if="liveData.charger?.npb450?.psu_mode_ok" variant="success" :show="true">
            {{ $t('home.PsuCommissionVerified') }}
        </BootstrapAlert>
        <BootstrapAlert
            v-else-if="liveData.charger?.npb450?.commissioning_requested"
            variant="warning"
            :show="true"
        >
            {{ $t('home.PsuCommissionPowerCycleRequired') }}
            <div class="mt-2 small">{{ $t('home.PsuCommissionNextPoll', { seconds: commissioningPollSeconds }) }}</div>
        </BootstrapAlert>
        <p>{{ $t('home.PsuCommissionExplanation') }}</p>
        <ol>
            <li>{{ $t('home.PsuCommissionStepSend') }}</li>
            <li>{{ $t('home.PsuCommissionStepPowerCycle') }}</li>
            <li>{{ $t('home.PsuCommissionStepVerify') }}</li>
        </ol>
        <div class="d-flex justify-content-end">
            <button
                v-if="!liveData.charger?.npb450?.psu_mode_ok && !liveData.charger?.npb450?.commissioning_requested"
                type="button"
                class="btn btn-warning"
                :disabled="sendingPsuCommission || !isLogged"
                @click="onCommissionPsuMode"
            >
                <span v-if="sendingPsuCommission" class="spinner-border spinner-border-sm" aria-hidden="true"></span>
                {{ sendingPsuCommission ? $t('home.Sending') : $t('home.SendPsuCommissionCommand') }}
            </button>
        </div>
    </ModalDialog>

    <ModalDialog modalId="eventView" :title="$t('home.EventLog')" :loading="eventLogLoading">
        <EventLog :eventLogList="eventLogList" />
    </ModalDialog>

    <ModalDialog modalId="devInfoView" :title="$t('home.InverterInfo')" :loading="devInfoLoading">
        <DevInfo :devInfoList="devInfoList" />
    </ModalDialog>

    <ModalDialog modalId="gridProfileView" :title="$t('home.GridProfile')" :loading="gridProfileLoading">
        <GridProfile :gridProfileList="gridProfileList" :gridProfileRawList="gridProfileRawList" />
    </ModalDialog>

    <ModalDialog modalId="limitSettingView" :title="$t('home.LimitSettings')" :loading="limitSettingLoading">
        <BootstrapAlert v-model="showAlertLimit" :variant="alertTypeLimit">
            {{ alertMessageLimit }}
        </BootstrapAlert>

        <div class="row mb-3">
            <label for="inputCurrentLimit" class="col-sm-3 col-form-label">{{ $t('home.CurrentLimit') }} </label>
            <div class="col-sm-4">
                <div class="input-group">
                    <input
                        type="text"
                        class="form-control"
                        id="inputCurrentLimit"
                        aria-describedby="currentLimitType"
                        v-model="currentLimitRelative"
                        disabled
                    />
                    <span class="input-group-text" id="currentLimitType">%</span>
                </div>
            </div>

            <div class="col-sm-4" v-if="currentLimitList.max_power > 0">
                <div class="input-group">
                    <input
                        type="text"
                        class="form-control"
                        id="inputCurrentLimitAbsolute"
                        aria-describedby="currentLimitTypeAbsolute"
                        v-model="currentLimitAbsolute"
                        disabled
                    />
                    <span class="input-group-text" id="currentLimitTypeAbsolute">W</span>
                </div>
            </div>
        </div>

        <div class="row mb-3 align-items-center">
            <label for="inputLastLimitSet" class="col-sm-3 col-form-label">
                {{ $t('home.LastLimitSetStatus') }}
            </label>
            <div class="col-sm-9">
                <span
                    class="badge"
                    :class="{
                        'text-bg-danger': currentLimitList.limit_set_status == 'Failure',
                        'text-bg-warning': currentLimitList.limit_set_status == 'Pending',
                        'text-bg-success': currentLimitList.limit_set_status == 'Ok',
                        'text-bg-secondary': currentLimitList.limit_set_status == 'Unknown',
                    }"
                >
                    {{ $t('home.' + currentLimitList.limit_set_status) }}
                </span>
            </div>
        </div>

        <div class="row mb-3">
            <label for="inputTargetLimit" class="col-sm-3 col-form-label">{{ $t('home.SetLimit') }}</label>
            <div class="col-sm-9">
                <div class="input-group">
                    <input
                        type="number"
                        name="inputTargetLimit"
                        class="form-control"
                        id="inputTargetLimit"
                        :min="targetLimitMin"
                        :max="targetLimitMax"
                        v-model="targetLimitList.limit_value"
                    />
                    <button
                        class="btn btn-primary dropdown-toggle"
                        type="button"
                        data-bs-toggle="dropdown"
                        aria-expanded="false"
                    >
                        {{ targetLimitTypeText }}
                    </button>
                    <ul class="dropdown-menu dropdown-menu-end">
                        <li>
                            <a class="dropdown-item" @click="onSelectType(true)" href="#">{{ $t('home.Relative') }}</a>
                        </li>
                        <li>
                            <a class="dropdown-item" @click="onSelectType(false)" href="#">{{ $t('home.Absolute') }}</a>
                        </li>
                    </ul>
                </div>
                <div
                    v-if="!targetLimitRelative"
                    class="alert alert-secondary mt-3"
                    role="alert"
                    v-html="$t('home.LimitHint')"
                ></div>
            </div>
        </div>

        <template #footer>
            <button type="button" class="btn btn-danger" @click="onSetLimitSettings(true)">
                {{ $t('home.SetPersistent') }}
            </button>

            <button type="button" class="btn btn-danger" @click="onSetLimitSettings(false)">
                {{ $t('home.SetNonPersistent') }}
            </button>
        </template>
    </ModalDialog>

    <ModalDialog modalId="powerSettingView" :title="$t('home.PowerSettings')" :loading="powerSettingLoading">
        <BootstrapAlert v-model="showAlertPower" :variant="alertTypePower">
            {{ alertMessagePower }}
        </BootstrapAlert>

        <div class="row mb-3 align-items-center">
            <label for="inputLastPowerSet" class="col col-form-label">{{ $t('home.LastPowerSetStatus') }}</label>
            <div class="col">
                <span
                    class="badge"
                    :class="{
                        'text-bg-danger': successCommandPower == 'Failure',
                        'text-bg-warning': successCommandPower == 'Pending',
                        'text-bg-success': successCommandPower == 'Ok',
                        'text-bg-secondary': successCommandPower == 'Unknown',
                    }"
                >
                    {{ $t('home.' + successCommandPower) }}
                </span>
            </div>
        </div>

        <div class="d-grid gap-2 col-6 mx-auto">
            <button type="button" class="btn btn-success" @click="onSetPowerSettings(true)">
                <BIconToggleOn class="fs-4" />&nbsp;{{ $t('home.TurnOn') }}
            </button>
            <button type="button" class="btn btn-danger" @click="onSetPowerSettings(false)">
                <BIconToggleOff class="fs-4" />&nbsp;{{ $t('home.TurnOff') }}
            </button>
            <button type="button" class="btn btn-warning" @click="onSetPowerSettings(true, true)">
                <BIconArrowCounterclockwise class="fs-4" />&nbsp;{{ $t('home.Restart') }}
            </button>
        </div>
    </ModalDialog>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import DataAgeDisplay from '@/components/DataAgeDisplay.vue';
import DevInfo from '@/components/DevInfo.vue';
import EventLog from '@/components/EventLog.vue';
import GridProfile from '@/components/GridProfile.vue';
import HintView from '@/components/HintView.vue';
import InverterChannelInfo from '@/components/InverterChannelInfo.vue';
import InverterTotalInfo from '@/components/InverterTotalInfo.vue';
import { LimitType } from '@/types/LimitConfig';
import ModalDialog from '@/components/ModalDialog.vue';
import type { DevInfoStatus } from '@/types/DevInfoStatus';
import type { EventlogItems } from '@/types/EventlogStatus';
import type { GridProfileRawdata } from '@/types/GridProfileRawdata';
import type { GridProfileStatus } from '@/types/GridProfileStatus';
import type { LimitConfig } from '@/types/LimitConfig';
import type { LimitStatus } from '@/types/LimitStatus';
import type { CanLogEntry, ChargerStatus, Inverter, LiveData } from '@/types/LiveDataStatus';
import { authHeader, authUrl, handleResponse, isLoggedIn } from '@/utils/authentication';
import * as bootstrap from 'bootstrap';
import {
    BIconArrowCounterclockwise,
    BIconBroadcast,
    BIconCpu,
    BIconInfoCircle,
    BIconJournalText,
    BIconOutlet,
    BIconPower,
    BIconSpeedometer,
    BIconToggleOff,
    BIconToggleOn,
} from 'bootstrap-icons-vue';
import { defineComponent } from 'vue';
import WebSocketService from '@/utils/websocketService';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        DataAgeDisplay,
        DevInfo,
        EventLog,
        GridProfile,
        HintView,
        InverterChannelInfo,
        InverterTotalInfo,
        ModalDialog,
        BIconArrowCounterclockwise,
        BIconBroadcast,
        BIconCpu,
        BIconInfoCircle,
        BIconJournalText,
        BIconOutlet,
        BIconPower,
        BIconSpeedometer,
        BIconToggleOff,
        BIconToggleOn,
    },
    data() {
        return {
            isLogged: isLoggedIn(),

            socket: {} as WebSocketService,
            heartInterval: 0,
            dataAgeTimers: {} as Record<string, number>,
            dataLoading: true,
            liveData: {} as LiveData,
            isFirstFetchAfterConnect: true,
            eventLogView: {} as bootstrap.Modal,
            eventLogList: {} as EventlogItems,
            eventLogLoading: true,
            devInfoView: {} as bootstrap.Modal,
            devInfoList: {} as DevInfoStatus,
            devInfoLoading: true,
            gridProfileView: {} as bootstrap.Modal,
            gridProfileList: {} as GridProfileStatus,
            gridProfileRawList: {} as GridProfileRawdata,
            gridProfileLoading: true,

            limitSettingView: {} as bootstrap.Modal,
            limitSettingLoading: true,

            currentLimitList: {} as LimitStatus,
            targetLimitList: {} as LimitConfig,

            targetLimitMin: 0,
            targetLimitMax: 100,
            targetLimitTypeText: this.$t('home.Relative'),
            targetLimitRelative: true,

            alertMessageLimit: '',
            alertTypeLimit: 'info',
            showAlertLimit: false,
            performRadioStatsReset: false,

            powerSettingView: {} as bootstrap.Modal,
            powerSettingSerial: '',
            powerSettingLoading: true,
            alertMessagePower: '',
            alertTypePower: 'info',
            showAlertPower: false,
            successCommandPower: '',
            chargerCanDebugView: {} as bootstrap.Modal,
            psuCommissionView: {} as bootstrap.Modal,
            chargerCanLoading: false,
            alertMessageCan: '',
            alertTypeCan: 'info',
            showAlertCan: false,
            sendingCanFrame: false,
            sendingPsuCommission: false,
            commissioningPollTick: 0,
            commissioningPollDeadlineMs: 0,
            selectedCanPreset: '',
            canFrameForm: {
                idHex: '0x000',
                dlc: 0,
                dataHex: '',
                ext: false,
                rtr: false,
            },
            canPresetOptions: [
                { id: 'npb_read_iout', label: 'NPB450: Read IOUT' },
                { id: 'npb_read_status', label: 'NPB450: Read System Status' },
                { id: 'npb_read_config', label: 'NPB450: Read System Config' },
                { id: 'npb_operation_on', label: 'NPB450: Operation ON' },
                { id: 'npb_operation_off', label: 'NPB450: Operation OFF' },
            ],

            isWebsocketConnected: false,
        };
    },
    created() {
        this.getInitialData();
        this.initSocket();
        this.$emitter.on('logged-in', () => {
            this.isLogged = this.isLoggedIn();
        });
        this.$emitter.on('logged-out', () => {
            this.isLogged = this.isLoggedIn();
        });
    },
    mounted() {
        this.eventLogView = new bootstrap.Modal('#eventView');
        this.devInfoView = new bootstrap.Modal('#devInfoView');
        this.gridProfileView = new bootstrap.Modal('#gridProfileView');
        this.limitSettingView = new bootstrap.Modal('#limitSettingView');
        this.powerSettingView = new bootstrap.Modal('#powerSettingView');
        this.chargerCanDebugView = new bootstrap.Modal('#chargerCanDebugView');
        this.psuCommissionView = new bootstrap.Modal('#psuCommissionView');
        window.setInterval(() => {
            this.commissioningPollTick++;
        }, 1000);
    },
    unmounted() {
        this.socket?.close();
    },
    updated() {
        console.log('Updated');
        // Select first tab
        if (this.isFirstFetchAfterConnect) {
            console.log('isFirstFetchAfterConnect');

            this.$nextTick(() => {
                console.log('nextTick');
                const firstTabEl = document.querySelector('#v-pills-tab:first-child button');
                if (firstTabEl != null) {
                    this.isFirstFetchAfterConnect = false;
                    console.log('Show');
                    const firstTab = new bootstrap.Tab(firstTabEl);
                    firstTab.show();
                }
            });
        }
    },
    computed: {
        currentLimitAbsolute(): string {
            if (this.currentLimitList.max_power > 0) {
                return this.$n(
                    (this.currentLimitList.limit_relative * this.currentLimitList.max_power) / 100,
                    'decimalNoDigits'
                );
            }
            return '0';
        },
        currentLimitRelative(): string {
            return this.$n(this.currentLimitList.limit_relative, 'decimalOneDigit');
        },
        inverterData(): Inverter[] {
            return this.liveData.inverters.slice().sort((a: Inverter, b: Inverter) => {
                return a.order - b.order;
            });
        },
        chargerCanLog(): CanLogEntry[] {
            const log = this.liveData.charger?.can_log || [];
            return log.slice(-5).reverse();
        },
        commissioningPollSeconds(): number {
            this.commissioningPollTick;
            return Math.max(0, Math.ceil((this.commissioningPollDeadlineMs - Date.now()) / 1000));
        },
    },
    methods: {
        isLoggedIn,
        updateCommissioningPollDeadline() {
            const milliseconds = this.liveData.charger?.npb450?.commissioning_next_poll_ms ?? 0;
            this.commissioningPollDeadlineMs = Date.now() + milliseconds;
        },
        getInitialData(triggerLoading: boolean = true) {
            if (triggerLoading) {
                this.dataLoading = true;
            }
            fetch('/api/livedata/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    if (!data.charger) {
                        data.charger = {
                            configured: false,
                            enabled: false,
                            data_age_ms: -1,
                            mcp2515: {},
                            npb450: {},
                            charger: {},
                            battery: {},
                            can_log: [],
                        } as ChargerStatus;
                    }
                    this.liveData = data;
                    this.updateCommissioningPollDeadline();
                    if (triggerLoading) {
                        this.dataLoading = false;
                    }
                });
        },
        reloadData() {
            this.socket?.close();

            this.getInitialData(false);
            this.initSocket();
        },
        handleMessage(event: MessageEvent) {
            if (!event.data || event.data === '{}') {
                this.socket?.close(); // force reconnect
                this.initSocket();
                return;
            }

            const newData = JSON.parse(event.data);

            Object.assign(this.liveData.total, newData.total);
            Object.assign(this.liveData.hints, newData.hints);
            if (newData.charger) {
                if (!this.liveData.charger) {
                    this.liveData.charger = {
                        configured: false,
                        enabled: false,
                        data_age_ms: -1,
                        mcp2515: {},
                        npb450: {},
                        charger: {},
                        battery: {},
                        can_log: [],
                    } as ChargerStatus;
                }
                if (!this.liveData.charger.mcp2515) {
                    this.liveData.charger.mcp2515 = {};
                }
                Object.assign(this.liveData.charger, newData.charger);
                Object.assign(this.liveData.charger.mcp2515, newData.charger.mcp2515 || {});
                Object.assign(this.liveData.charger.npb450, newData.charger.npb450 || {});
                Object.assign(this.liveData.charger.charger, newData.charger.charger || {});
                Object.assign(this.liveData.charger.battery, newData.charger.battery || {});
                this.liveData.charger.can_log = newData.charger.can_log || [];
                this.updateCommissioningPollDeadline();
            }

            const idx = this.liveData.inverters.findIndex((i) => i.serial === newData.inverters[0].serial);

            if (idx == -1) {
                Object.assign(this.liveData.inverters, newData.inverters);
                this.liveData.inverters.forEach((inv) => this.resetDataAging(inv));
            } else if (this.liveData.inverters[idx]) {
                Object.assign(this.liveData.inverters[idx], newData.inverters[0]);
                this.resetDataAging(this.liveData.inverters[idx]);
            }
        },
        initSocket() {
            console.log('Starting connection to WebSocket Server');

            const { protocol, host } = location;
            const authString = authUrl();
            const webSocketUrl = `${protocol === 'https:' ? 'wss' : 'ws'}://${authString}${host}/livedata`;

            this.socket = new WebSocketService(webSocketUrl, {
                onMessage: this.handleMessage,
                onOpen: () => {
                    console.log('WebSocket connected');
                    this.isWebsocketConnected = true;
                },
                onClose: () => {
                    console.log('WebSocket closed');
                    this.isWebsocketConnected = false;
                },
            });

            // Listen to window events , When the window closes , Take the initiative to disconnect websocket Connect
            window.onbeforeunload = () => {
                this.socket?.close();
            };

            this.socket?.connect();
        },
        resetDataAging(inv: Inverter) {
            if (this.dataAgeTimers[inv.serial] !== undefined) {
                clearTimeout(this.dataAgeTimers[inv.serial]);
            }

            const nextMs = 1000 - (inv.data_age_ms % 1000);
            this.dataAgeTimers[inv.serial] = setTimeout(() => {
                this.doDataAging(inv.serial);
            }, nextMs);
        },
        doDataAging(serial: string) {
            const inv = this.liveData?.inverters?.find((inv) => inv.serial === serial);
            if (inv === undefined) {
                return;
            }

            inv.data_age_ms += 1000;

            this.dataAgeTimers[serial] = setTimeout(() => {
                this.doDataAging(serial);
            }, 1000);
        },
        onShowEventlog(serial: string) {
            this.eventLogLoading = true;
            fetch('/api/eventlog/status?inv=' + serial + '&locale=' + this.$i18n.locale, {
                headers: authHeader(),
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.eventLogList = data;
                    this.eventLogLoading = false;
                });

            this.eventLogView.show();
        },
        onShowDevInfo(serial: string) {
            this.devInfoLoading = true;
            fetch('/api/devinfo/status?inv=' + serial, { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.devInfoList = data;
                    this.devInfoList.serial = serial;
                    this.devInfoLoading = false;
                });

            this.devInfoView.show();
        },
        onShowGridProfile(serial: string) {
            this.gridProfileLoading = true;
            fetch('/api/gridprofile/status?inv=' + serial, { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.gridProfileList = data;

                    fetch('/api/gridprofile/rawdata?inv=' + serial, { headers: authHeader() })
                        .then((response) => handleResponse(response, this.$emitter, this.$router))
                        .then((data) => {
                            this.gridProfileRawList = data;
                            this.gridProfileLoading = false;
                        });
                });

            this.gridProfileView.show();
        },
        onShowLimitSettings(serial: string) {
            this.showAlertLimit = false;
            this.targetLimitList.serial = '';
            this.targetLimitList.limit_value = 0;
            this.onSelectType(true);

            this.limitSettingLoading = true;
            fetch('/api/limit/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.currentLimitList = data[serial];
                    this.targetLimitList.serial = serial;
                    this.limitSettingLoading = false;
                });

            this.limitSettingView.show();
        },
        onResetRadioStats(serial: string) {
            this.performRadioStatsReset = true;
            fetch('/api/inverter/stats_reset?inv=' + serial, { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then(() => {
                    this.performRadioStatsReset = false;
                });
        },
        onSetLimitSettings(setPersistent: boolean) {
            if (setPersistent) {
                if (this.targetLimitRelative) {
                    this.targetLimitList.limit_type = LimitType.RelativPersistent;
                } else {
                    this.targetLimitList.limit_type = LimitType.AbsolutPersistent;
                }
            } else {
                if (this.targetLimitRelative) {
                    this.targetLimitList.limit_type = LimitType.RelativNonPersistent;
                } else {
                    this.targetLimitList.limit_type = LimitType.AbsolutNonPersistent;
                }
            }
            const formData = new FormData();
            formData.append('data', JSON.stringify(this.targetLimitList));

            console.log(this.targetLimitList);

            fetch('/api/limit/config', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    if (response.type == 'success') {
                        this.limitSettingView.hide();
                    } else {
                        this.alertMessageLimit = this.$t('apiresponse.' + response.code, response.param);
                        this.alertTypeLimit = response.type;
                        this.showAlertLimit = true;
                    }
                });
        },
        onSelectType(isRelative: boolean) {
            if (isRelative) {
                this.targetLimitTypeText = this.$t('home.Relative');
                this.targetLimitMin = 0;
                this.targetLimitMax = 100;
            } else {
                this.targetLimitTypeText = this.$t('home.Absolute');
                this.targetLimitMin = 0;
                this.targetLimitMax = this.currentLimitList.max_power > 0 ? this.currentLimitList.max_power : 2250;
            }
            this.targetLimitRelative = isRelative;
        },

        onShowPowerSettings(serial: string) {
            this.showAlertPower = false;
            this.powerSettingSerial = '';
            this.powerSettingLoading = true;
            fetch('/api/power/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.successCommandPower = data[serial].power_set_status;
                    this.powerSettingSerial = serial;
                    this.powerSettingLoading = false;
                });
            this.powerSettingView.show();
        },

        onSetPowerSettings(turnOn: boolean, restart = false) {
            const data = restart
                ? {
                      serial: this.powerSettingSerial,
                      restart: true,
                  }
                : {
                      serial: this.powerSettingSerial,
                      power: turnOn,
                  };

            const formData = new FormData();
            formData.append('data', JSON.stringify(data));

            console.log(data);

            fetch('/api/power/config', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    if (response.type == 'success') {
                        this.powerSettingView.hide();
                    } else {
                        this.alertMessagePower = this.$t('apiresponse.' + response.code, response.param);
                        this.alertTypePower = response.type;
                        this.showAlertPower = true;
                    }
                });
        },
        openCanDialog() {
            this.showAlertCan = false;
            this.chargerCanDebugView.show();
        },
        openPsuCommissionDialog() {
            this.psuCommissionView.show();
        },
        onCommissionPsuMode() {
            const address = this.liveData.charger?.npb450?.address ?? 0;
            const controllerId = 0x000c0100 + Math.min(Math.max(address, 0), 3);
            const formData = new FormData();
            formData.append(
                'data',
                JSON.stringify({
                    id: controllerId,
                    ext: true,
                    rtr: false,
                    dlc: 4,
                    data: [0xb4, 0x00, 0x04, 0x00],
                }),
            );

            this.sendingPsuCommission = true;
            fetch('/api/livedata/charger/can_tx', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .finally(() => {
                    this.sendingPsuCommission = false;
                });
        },
        onApplyCanPreset() {
            const address = this.liveData.charger?.npb450?.address ?? 0;
            const controllerId = 0x000c0100 + Math.min(Math.max(address, 0), 3);
            const applyPreset = (cmd: number, value?: number) => {
                this.canFrameForm.ext = true;
                this.canFrameForm.rtr = false;
                this.canFrameForm.idHex = `0x${controllerId.toString(16).toUpperCase()}`;
                const bytes = [cmd & 0xff, (cmd >> 8) & 0xff];
                if (value !== undefined) {
                    bytes.push(value & 0xff);
                    if (cmd !== 0x0000) {
                        bytes.push((value >> 8) & 0xff);
                    }
                }
                this.canFrameForm.dlc = bytes.length;
                this.canFrameForm.dataHex = bytes.map((v) => v.toString(16).toUpperCase().padStart(2, '0')).join(' ');
            };

            switch (this.selectedCanPreset) {
                case 'npb_read_iout':
                    applyPreset(0x0061);
                    break;
                case 'npb_read_status':
                    applyPreset(0x00c1);
                    break;
                case 'npb_read_config':
                    applyPreset(0x00c2);
                    break;
                case 'npb_operation_on':
                    applyPreset(0x0000, 0x0001);
                    break;
                case 'npb_operation_off':
                    applyPreset(0x0000, 0x0000);
                    break;
                default:
                    break;
            }
        },
        parseCanDataBytes(dataHex: string): number[] {
            const cleaned = dataHex.trim();
            if (cleaned.length === 0) {
                return [];
            }

            return cleaned
                .split(/[\s,;:]+/)
                .filter((token) => token.length > 0)
                .map((token) => {
                    const normalized = token.toLowerCase().startsWith('0x') ? token.slice(2) : token;
                    if (!/^[0-9a-fA-F]{1,2}$/.test(normalized)) {
                        throw new Error(String(this.$t('home.InvalidCanDataByte')));
                    }
                    return parseInt(normalized, 16);
                });
        },
        onSendCanFrame() {
            this.showAlertCan = false;

            const idText = this.canFrameForm.idHex.trim();
            const normalizedId = idText.toLowerCase().startsWith('0x') ? idText.slice(2) : idText;
            if (!/^[0-9a-fA-F]{1,8}$/.test(normalizedId)) {
                this.alertMessageCan = this.$t('home.InvalidCanId');
                this.alertTypeCan = 'danger';
                this.showAlertCan = true;
                return;
            }

            const frameId = parseInt(normalizedId, 16);
            if ((!this.canFrameForm.ext && frameId > 0x7ff) || (this.canFrameForm.ext && frameId > 0x1fffffff)) {
                this.alertMessageCan = this.$t('home.InvalidCanId');
                this.alertTypeCan = 'danger';
                this.showAlertCan = true;
                return;
            }

            let dataBytes: number[] = [];
            try {
                dataBytes = this.parseCanDataBytes(this.canFrameForm.dataHex);
            } catch (error) {
                this.alertMessageCan = String(error);
                this.alertTypeCan = 'danger';
                this.showAlertCan = true;
                return;
            }

            const dlc = Math.min(8, Math.max(0, Number(this.canFrameForm.dlc) || 0));
            if (dataBytes.length < dlc && !this.canFrameForm.rtr) {
                this.alertMessageCan = this.$t('home.NotEnoughCanData');
                this.alertTypeCan = 'danger';
                this.showAlertCan = true;
                return;
            }

            const payload = {
                id: frameId,
                ext: this.canFrameForm.ext,
                rtr: this.canFrameForm.rtr,
                dlc,
                data: dataBytes.slice(0, dlc),
            };

            const formData = new FormData();
            formData.append('data', JSON.stringify(payload));

            this.sendingCanFrame = true;
            fetch('/api/livedata/charger/can_tx', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.alertMessageCan = this.$t('apiresponse.' + response.code, response.param);
                    this.alertTypeCan = response.type;
                    this.showAlertCan = true;
                })
                .finally(() => {
                    this.sendingCanFrame = false;
                });
        },
        getSumIrridiation(inv: Inverter): number {
            let total = 0;
            Object.keys(inv.DC).forEach((key) => {
                total += inv.DC[key as unknown as number]?.Irradiation?.max || 0;
            });
            return total;
        },
        ratio(val_small: number, val_large: number): string {
            if (val_large == 0) {
                return '-';
            }
            return this.$n(val_small / val_large, 'percent');
        },
        chargerInitClass(initState?: string): string {
            switch (initState) {
                case 'ready':
                    return 'text-bg-success';
                case 'fault':
                    return 'text-bg-danger';
                case 'request_validation':
                case 'set_eeprom_lock':
                    return 'text-bg-warning';
                default:
                    return 'text-bg-secondary';
            }
        },
        formatWord(value?: number): string {
            if (value === undefined) {
                return '-';
            }
            return `0x${value.toString(16).toUpperCase().padStart(4, '0')} (${value})`;
        },
        formatCanId(id: number, ext: boolean): string {
            const width = ext ? 8 : 3;
            return `0x${id.toString(16).toUpperCase().padStart(width, '0')}`;
        },
        formatCanData(data: number[], dlc: number): string {
            return (data || [])
                .slice(0, dlc)
                .map((v) => v.toString(16).toUpperCase().padStart(2, '0'))
                .join(' ');
        },
        formatCanFlags(entry: CanLogEntry): string {
            const flags: string[] = [];
            flags.push(entry.tx ? 'TX' : 'RX');
            flags.push(entry.ext ? 'EXT' : 'STD');
            if (entry.rtr) {
                flags.push('RTR');
            }
            flags.push(`DLC${entry.dlc}`);
            return flags.join(' ');
        },
    },
});
</script>

<style scoped>
.btn-group {
    border-radius: var(--bs-border-radius);
    margin-top: 0.25rem;
}

.can-log-dialog-table {
    max-height: 320px;
    overflow-y: auto;
}
</style>
