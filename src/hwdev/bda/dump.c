/*
 * Astra Module: BDA (Request dumping)
 *
 * Copyright (C) 2016, Artem Kharitonov <artem@3phase.pw>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bda.h"

#define BDA_DUMP(...) \
    do { \
        asc_log_debug("[dvb_input] " __VA_ARGS__); \
    } while (0)

/* dump ITuningSpace properties */
static
void dump_space(ITuningSpace *space)
{
    HRESULT hr = E_FAIL;
    long l;

    if (space == NULL)
        return;

    /* ITuningSpace::UniqueName */
    BSTR name = NULL;
    hr = ITuningSpace_get_UniqueName(space, &name);
    if (SUCCEEDED(hr) && name != NULL)
    {
        char *const buf = cx_narrow(name);
        SysFreeString(name);
        BDA_DUMP("ITuningSpace::UniqueName = %s", buf);
        free(buf);
    }

    /* IATSCTuningSpace */
    IATSCTuningSpace *space_atsc = NULL;
    hr = ITuningSpace_QueryInterface(space, &IID_IATSCTuningSpace
                                     , (void **)&space_atsc);

    if (SUCCEEDED(hr) && space_atsc != NULL)
    {
        BDA_DUMP("Tuning space supports IATSCTuningSpace");

        /* IATSCTuningSpace::MaxMinorChannel */
        hr = IATSCTuningSpace_get_MaxMinorChannel(space_atsc, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IATSCTuningSpace::MaxMinorChannel = %ld", l);

        /* IATSCTuningSpace::MaxPhysicalChannel */
        hr = IATSCTuningSpace_get_MaxPhysicalChannel(space_atsc, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IATSCTuningSpace::MaxPhysicalChannel = %ld", l);

        /* IATSCTuningSpace::MinMinorChannel */
        hr = IATSCTuningSpace_get_MinMinorChannel(space_atsc, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IATSCTuningSpace::MinMinorChannel = %ld", l);

        /* IATSCTuningSpace::MinPhysicalChannel */
        hr = IATSCTuningSpace_get_MinPhysicalChannel(space_atsc, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IATSCTuningSpace::MinPhysicalChannel = %ld", l);
    }

    SAFE_RELEASE(space_atsc);

    /* IAnalogTVTuningSpace */
    IAnalogTVTuningSpace *space_analog = NULL;
    hr = ITuningSpace_QueryInterface(space, &IID_IAnalogTVTuningSpace
                                     , (void **)&space_analog);

    if (SUCCEEDED(hr) && space_analog != NULL)
    {
        TunerInputType it;
        BDA_DUMP("Tuning space supports IAnalogTVTuningSpace");

        /* IAnalogTVTuningSpace::CountryCode */
        hr = IAnalogTVTuningSpace_get_CountryCode(space_analog, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IAnalogTVTuningSpace::CountryCode = %ld", l);

        /* IAnalogTVTuningSpace::InputType */
        hr = IAnalogTVTuningSpace_get_InputType(space_analog, &it);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IAnalogTVTuningSpace::InputType = %d", it);

        /* IAnalogTVTuningSpace::MaxChannel */
        hr = IAnalogTVTuningSpace_get_MaxChannel(space_analog, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IAnalogTVTuningSpace::MaxChannel = %ld", l);

        /* IAnalogTVTuningSpace::MinChannel */
        hr = IAnalogTVTuningSpace_get_MinChannel(space_analog, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IAnalogTVTuningSpace::MinChannel = %ld", l);
    }

    SAFE_RELEASE(space_analog);

    /* IDigitalCableTuningSpace */
    IDigitalCableTuningSpace *space_cqam = NULL;
    hr = ITuningSpace_QueryInterface(space, &IID_IDigitalCableTuningSpace
                                     , (void **)&space_cqam);

    if (SUCCEEDED(hr) && space_cqam != NULL)
    {
        BDA_DUMP("Tuning space supports IDigitalCableTuningSpace");

        /* IDigitalCableTuningSpace::MaxMajorChannel */
        hr = space_cqam->lpVtbl->get_MaxMajorChannel(space_cqam, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDigitalCableTuningSpace::MaxMajorChannel = %ld", l);

        /* IDigitalCableTuningSpace::MaxSourceID */
        hr = space_cqam->lpVtbl->get_MaxSourceID(space_cqam, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDigitalCableTuningSpace::MaxSourceID = %ld", l);

        /* IDigitalCableTuningSpace::MinMajorChannel */
        hr = space_cqam->lpVtbl->get_MinMajorChannel(space_cqam, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDigitalCableTuningSpace::MinMajorChannel = %ld", l);

        /* IDigitalCableTuningSpace::MinSourceID */
        hr = space_cqam->lpVtbl->get_MinSourceID(space_cqam, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDigitalCableTuningSpace::MinSourceID = %ld", l);
    }

    SAFE_RELEASE(space_cqam);

    /* IDVBSTuningSpace */
    IDVBSTuningSpace *space_dvbs = NULL;
    hr = ITuningSpace_QueryInterface(space, &IID_IDVBSTuningSpace
                                     , (void **)&space_dvbs);

    if (SUCCEEDED(hr) && space_dvbs != NULL)
    {
        SpectralInversion si;
        BSTR range = NULL;

        BDA_DUMP("Tuning space supports IDVBSTuningSpace");

        /* IDVBSTuningSpace::HighOscillator */
        hr = space_dvbs->lpVtbl->get_HighOscillator(space_dvbs, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSTuningSpace::HighOscillator = %ld", l);

        /* IDVBSTuningSpace::InputRange */
        hr = space_dvbs->lpVtbl->get_InputRange(space_dvbs, &range);
        if (SUCCEEDED(hr) && range != NULL)
        {
            char *const buf = cx_narrow(range);
            SysFreeString(range);
            BDA_DUMP("  IDVBSTuningSpace::InputRange = '%s'", buf);
            free(buf);
        }

        /* IDVBSTuningSpace::LNBSwitch */
        hr = space_dvbs->lpVtbl->get_LNBSwitch(space_dvbs, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSTuningSpace::LNBSwitch = %ld", l);

        /* IDVBSTuningSpace::LowOscillator */
        hr = space_dvbs->lpVtbl->get_LowOscillator(space_dvbs, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSTuningSpace::LowOscillator = %ld", l);

        /* IDVBSTuningSpace::SpectralInversion */
        hr = space_dvbs->lpVtbl->get_SpectralInversion(space_dvbs, &si);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSTuningSpace::SpectralInversion = %d", si);
    }

    SAFE_RELEASE(space_dvbs);

    /* IDVBTuningSpace */
    IDVBTuningSpace *space_dvb = NULL;
    hr = ITuningSpace_QueryInterface(space, &IID_IDVBTuningSpace
                                     , (void **)&space_dvb);

    if (SUCCEEDED(hr) && space_dvb != NULL)
    {
        DVBSystemType st;
        BDA_DUMP("Tuning space supports IDVBTuningSpace");

        /* IDVBTuningSpace::SystemType */
        hr = space_dvb->lpVtbl->get_SystemType(space_dvb, &st);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTuningSpace::SystemType = %d", st);
    }

    SAFE_RELEASE(space_dvb);

    /* IDVBTuningSpace2 */
    IDVBTuningSpace2 *space_dvb2 = NULL;
    hr = ITuningSpace_QueryInterface(space, &IID_IDVBTuningSpace2
                                     , (void **)&space_dvb2);

    if (SUCCEEDED(hr) && space_dvb2 != NULL)
    {
        BDA_DUMP("Tuning space supports IDVBTuningSpace2");

        /* IDVBTuningSpace2::NetworkID */
        hr = space_dvb2->lpVtbl->get_NetworkID(space_dvb2, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTuningSpace2::NetworkID = %ld", l);
    }

    SAFE_RELEASE(space_dvb2);
}

/* dump ILocator properties */
static
void dump_locator(ILocator *locator)
{
    HRESULT hr = E_FAIL;

    long l;
    ModulationType modval;
    BinaryConvolutionCodeRate fecval;
    FECMethod fecmodeval;

    if (locator == NULL)
        return;

    BDA_DUMP("ILocator properties");

    /* ILocator::CarrierFrequency */
    hr = ILocator_get_CarrierFrequency(locator, &l);
    if (SUCCEEDED(hr))
        BDA_DUMP("  ILocator::CarrierFrequency = %ld", l);

    /* ILocator::InnerFEC */
    hr = ILocator_get_InnerFEC(locator, &fecmodeval);
    if (SUCCEEDED(hr))
        BDA_DUMP("  ILocator::InnerFEC = %d", fecmodeval);

    /* ILocator::InnerFECRate */
    hr = ILocator_get_InnerFECRate(locator, &fecval);
    if (SUCCEEDED(hr))
        BDA_DUMP("  ILocator::InnerFECRate = %d", fecval);

    /* ILocator::Modulation */
    hr = ILocator_get_Modulation(locator, &modval);
    if (SUCCEEDED(hr))
        BDA_DUMP("  ILocator::Modulation = %d", modval);

    /* ILocator::OuterFEC */
    hr = ILocator_get_OuterFEC(locator, &fecmodeval);
    if (SUCCEEDED(hr))
        BDA_DUMP("  ILocator::OuterFEC = %d", fecmodeval);

    /* ILocator::OuterFECRate */
    hr = ILocator_get_OuterFECRate(locator, &fecval);
    if (SUCCEEDED(hr))
        BDA_DUMP("  ILocator::OuterFECRate = %d", fecval);

    /* ILocator::SymbolRate */
    hr = ILocator_get_SymbolRate(locator, &l);
    if (SUCCEEDED(hr))
        BDA_DUMP("  ILocator::SymbolRate = %ld", l);

    /* IATSCLocator2 */
    IATSCLocator2 *locator_atsc2 = NULL;
    hr = ILocator_QueryInterface(locator, &IID_IATSCLocator2
                                 , (void **)&locator_atsc2);

    if (SUCCEEDED(hr) && locator_atsc2 != NULL)
    {
        BDA_DUMP("Locator supports IATSCLocator2");

        /* IATSCLocator2::ProgramNumber */
        hr = IATSCLocator2_get_ProgramNumber(locator_atsc2, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IATSCLocator2::ProgramNumber = %ld", l);
    }

    SAFE_RELEASE(locator_atsc2);

    /* IATSCLocator */
    IATSCLocator *locator_atsc = NULL;
    hr = ILocator_QueryInterface(locator, &IID_IATSCLocator
                                 , (void **)&locator_atsc);

    if (SUCCEEDED(hr) && locator_atsc != NULL)
    {
        BDA_DUMP("Locator supports IATSCLocator");

        /* IATSCLocator::PhysicalChannel */
        hr = IATSCLocator_get_PhysicalChannel(locator_atsc, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IATSCLocator::PhysicalChannel = %ld", l);

        /* IATSCLocator::TSID */
        hr = IATSCLocator_get_TSID(locator_atsc, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IATSCLocator::TSID = %ld", l);
    }

    SAFE_RELEASE(locator_atsc);

    /* IDVBSLocator */
    IDVBSLocator *locator_dvbs = NULL;
    hr = ILocator_QueryInterface(locator, &IID_IDVBSLocator
                                 , (void **)&locator_dvbs);

    if (SUCCEEDED(hr) && locator_dvbs != NULL)
    {
        Polarisation pol;
        VARIANT_BOOL west;

        BDA_DUMP("Locator supports IDVBSLocator");

        /* IDVBSLocator::Azimuth */
        hr = locator_dvbs->lpVtbl->get_Azimuth(locator_dvbs, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator::Azimuth = %ld", l);

        /* IDVBSLocator::Elevation */
        hr = locator_dvbs->lpVtbl->get_Elevation(locator_dvbs, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator::Elevation = %ld", l);

        /* IDVBSLocator::OrbitalPosition */
        hr = locator_dvbs->lpVtbl->get_OrbitalPosition(locator_dvbs, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator::OrbitalPosition = %ld", l);

        /* IDVBSLocator::SignalPolarisation */
        hr = locator_dvbs->lpVtbl->get_SignalPolarisation(locator_dvbs, &pol);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator::SignalPolarisation = %d", pol);

        /* IDVBSLocator::WestPosition */
        hr = locator_dvbs->lpVtbl->get_WestPosition(locator_dvbs, &west);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator::WestPosition = %d", west);
    }

    SAFE_RELEASE(locator_dvbs);

    /* IDVBSLocator2 */
    IDVBSLocator2 *locator_dvbs2 = NULL;
    hr = ILocator_QueryInterface(locator, &IID_IDVBSLocator2
                                 , (void **)&locator_dvbs2);

    if (SUCCEEDED(hr) && locator_dvbs2 != NULL)
    {
        Pilot sp;
        RollOff sr;
        SpectralInversion si;
        LNB_Source ls;

        BDA_DUMP("Locator supports IDVBSLocator2");

        /* IDVBSLocator2::DiseqLNBSource */
        hr = locator_dvbs2->lpVtbl->get_DiseqLNBSource(locator_dvbs2, &ls);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator2::DiseqLNBSource = %d", ls);

        /* IDVBSLocator2::LocalLNBSwitchOverride */
        hr = locator_dvbs2->lpVtbl->get_LocalLNBSwitchOverride(
                                                        locator_dvbs2, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator2::LocalLNBSwitchOverride = %ld", l);

        /* IDVBSLocator2::LocalOscillatorOverrideHigh */
        hr = locator_dvbs2->lpVtbl->get_LocalOscillatorOverrideHigh(
                                                        locator_dvbs2, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator2::LocalOscillatorOverrideHigh = %ld", l);

        /* IDVBSLocator2::LocalOscillatorOverrideLow */
        hr = locator_dvbs2->lpVtbl->get_LocalOscillatorOverrideLow(
                                                        locator_dvbs2, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator2::LocalOscillatorOverrideLow = %ld", l);

        /* IDVBSLocator2::LocalSpectralInversionOverride */
        hr = locator_dvbs2->lpVtbl->get_LocalSpectralInversionOverride(
                                                        locator_dvbs2, &si);
        if (SUCCEEDED(hr))
        {
            BDA_DUMP("  IDVBSLocator2::LocalSpectralInversionOverride = %d"
                     , si);
        }

        /* IDVBSLocator2::SignalPilot */
        hr = locator_dvbs2->lpVtbl->get_SignalPilot(locator_dvbs2, &sp);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator2::SignalPilot = %d", sp);

        /* IDVBSLocator2::SignalRollOff */
        hr = locator_dvbs2->lpVtbl->get_SignalRollOff(locator_dvbs2, &sr);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBSLocator2::SignalRollOff = %d", sr);
    }

    SAFE_RELEASE(locator_dvbs2);

    /* IDVBTLocator */
    IDVBTLocator *locator_dvbt = NULL;
    hr = ILocator_QueryInterface(locator, &IID_IDVBTLocator
                                 , (void **)&locator_dvbt);

    if (SUCCEEDED(hr) && locator_dvbt != NULL)
    {
        GuardInterval gu;
        HierarchyAlpha ha;
        FECMethod fecmode;
        BinaryConvolutionCodeRate fec;
        TransmissionMode mode;
        VARIANT_BOOL oth;

        BDA_DUMP("Locator supports IDVBTLocator");

        /* IDVBTLocator::Bandwidth */
        hr = locator_dvbt->lpVtbl->get_Bandwidth(locator_dvbt, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTLocator::Bandwidth = %ld", l);

        /* IDVBTLocator::Guard */
        hr = locator_dvbt->lpVtbl->get_Guard(locator_dvbt, &gu);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTLocator::Guard = %d", gu);

        /* IDVBTLocator::HAlpha */
        hr = locator_dvbt->lpVtbl->get_HAlpha(locator_dvbt, &ha);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTLocator::HAlpha = %d", ha);

        /* IDVBTLocator::LPInnerFEC */
        hr = locator_dvbt->lpVtbl->get_LPInnerFEC(locator_dvbt, &fecmode);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTLocator::LPInnerFEC = %d", fecmode);

        /* IDVBTLocator::LPInnerFECRate */
        hr = locator_dvbt->lpVtbl->get_LPInnerFECRate(locator_dvbt, &fec);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTLocator::LPInnerFECRate = %d", fec);

        /* IDVBTLocator::Mode */
        hr = locator_dvbt->lpVtbl->get_Mode(locator_dvbt, &mode);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTLocator::Mode = %d", mode);

        /* IDVBTLocator::OtherFrequencyInUse */
        hr = locator_dvbt->lpVtbl->get_OtherFrequencyInUse(locator_dvbt, &oth);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTLocator::OtherFrequencyInUse = %d", oth);
    }

    SAFE_RELEASE(locator_dvbt);

    /* IDVBTLocator2 */
    IDVBTLocator2 *locator_dvbt2 = NULL;
    hr = ILocator_QueryInterface(locator, &IID_IDVBTLocator2
                                 , (void **)&locator_dvbt2);

    if (SUCCEEDED(hr) && locator_dvbt2 != NULL)
    {
        BDA_DUMP("Locator supports IDVBTLocator2");

        /* IDVBTLocator2::PhysicalLayerPipeId */
        hr = locator_dvbt2->lpVtbl->get_PhysicalLayerPipeId(locator_dvbt2, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTLocator2::PhysicalLayerPipeId = %ld", l);
    }

    SAFE_RELEASE(locator_dvbt2);
}

/* dump ITuneRequest properties */
static
void dump_request(ITuneRequest *request)
{
    HRESULT hr = E_FAIL;
    long l;

    /* IATSCChannelTuneRequest */
    IATSCChannelTuneRequest *request_atsc = NULL;
    hr = ITuneRequest_QueryInterface(request, &IID_IATSCChannelTuneRequest
                                     , (void **)&request_atsc);

    if (SUCCEEDED(hr) && request_atsc != NULL)
    {
        BDA_DUMP("Tune request supports IATSCChannelTuneRequest");

        /* IATSCChannelTuneRequest::MinorChannel */
        hr = request_atsc->lpVtbl->get_MinorChannel(request_atsc, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IATSCChannelTuneRequest::MinorChannel = %ld", l);
    }

    SAFE_RELEASE(request_atsc);

    /* IChannelTuneRequest */
    IChannelTuneRequest *request_ch = NULL;
    hr = ITuneRequest_QueryInterface(request, &IID_IChannelTuneRequest
                                     , (void **)&request_ch);

    if (SUCCEEDED(hr) && request_ch != NULL)
    {
        BDA_DUMP("Tune request supports IChannelTuneRequest");

        /* IChannelTuneRequest::Channel */
        hr = request_ch->lpVtbl->get_Channel(request_ch, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IChannelTuneRequest::Channel = %ld", l);
    }

    SAFE_RELEASE(request_ch);

    /* IDigitalCableTuneRequest */
    IDigitalCableTuneRequest *request_cqam = NULL;
    hr = ITuneRequest_QueryInterface(request, &IID_IDigitalCableTuneRequest
                                     , (void **)&request_cqam);

    if (SUCCEEDED(hr) && request_cqam != NULL)
    {
        BDA_DUMP("Tune request supports IDigitalCableTuneRequest");

        /* IDigitalCableTuneRequest::MajorChannel */
        hr = request_cqam->lpVtbl->get_MajorChannel(request_cqam, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDigitalCableTuneRequest::MajorChannel = %ld", l);

        /* IDigitalCableTuneRequest::SourceID */
        hr = request_cqam->lpVtbl->get_SourceID(request_cqam, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDigitalCableTuneRequest::SourceID = %ld", l);
    }

    SAFE_RELEASE(request_cqam);

    /* IDVBTuneRequest */
    IDVBTuneRequest *request_dvb = NULL;
    hr = ITuneRequest_QueryInterface(request, &IID_IDVBTuneRequest
                                     , (void **)&request_dvb);

    if (SUCCEEDED(hr) && request_dvb != NULL)
    {
        BDA_DUMP("Tune request supports IDVBTuneRequest");

        /* IDVBTuneRequest::ONID */
        hr = request_dvb->lpVtbl->get_ONID(request_dvb, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTuneRequest::ONID = %ld", l);

        /* IDVBTuneRequest::SID */
        hr = request_dvb->lpVtbl->get_SID(request_dvb, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTuneRequest::SID = %ld", l);

        /* IDVBTuneRequest::TSID */
        hr = request_dvb->lpVtbl->get_TSID(request_dvb, &l);
        if (SUCCEEDED(hr))
            BDA_DUMP("  IDVBTuneRequest::TSID = %ld", l);
    }

    SAFE_RELEASE(request_dvb);
}

/* dump tune request */
void bda_dump_request(ITuneRequest *request)
{
    HRESULT hr = E_FAIL;

    if (request == NULL)
        return;

    BDA_DUMP("begin tune request dump");

    /* ITuningSpace */
    ITuningSpace *space = NULL;
    hr = ITuneRequest_get_TuningSpace(request, &space);
    if (SUCCEEDED(hr) && space != NULL)
        dump_space(space);

    SAFE_RELEASE(space);

    /* ILocator */
    ILocator *locator = NULL;
    hr = ITuneRequest_get_Locator(request, &locator);
    if (SUCCEEDED(hr) && locator != NULL)
        dump_locator(locator);

    SAFE_RELEASE(locator);

    /* ITuneRequest */
    dump_request(request);

    BDA_DUMP("end tune request dump");
}
