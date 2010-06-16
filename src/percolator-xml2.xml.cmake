<?xml version="1.0" encoding="UTF-8"?>
<experiment  xmlns="@percolator-in-namespace@" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://per-colator.com/percolator_in/10 file://@CMAKE_CURRENT_SOURCE_DIR@/percolator-xml.xsd" num_features="7" >
    <enzyme>myEnzyme</enzyme>
    <calibration>
      <calibration_parameter name="myParam1" value="2.3"/>
      <calibration_parameter name="myParam2" value="1.2"/>
      <mass_type>average</mass_type>
    </calibration>
    <feature_descriptions>
       <feature_description>myFeature1</feature_description>
       <feature_description>myFeature2</feature_description>
       <feature_description>myFeature3</feature_description>
       <feature_description>myFeature4</feature_description>
       <feature_description>myFeature5</feature_description>
       <feature_description>myFeature6</feature_description>
       <feature_description>myFeature7</feature_description>
    </feature_descriptions>
    <frag_spectrum_scan num="2">
      <observed mass_charge="2.1"/>
      <ion_current val="5.6"></ion_current>
          <peptide_spectrum_match charge="12" type="target" id="peptide_spectrum_match1">
            <features>
              <feature>2.13</feature>
              <feature>2.14</feature>
              <feature>3.13</feature>
              <feature>2.13</feature>
              <feature>2.13</feature>
              <feature>4.13</feature>
              <feature>8.17</feature>
            </features>
              <calculated mass_charge="3.4"/>
              <peptide>N.QRLKNGN.K</peptide>
            <protein id="genA"/>
            <protein id="genB"/>
          </peptide_spectrum_match>
          <peptide_spectrum_match charge="14" type="decoy"  id="peptide_spectrum_match2">
            <features>
              <feature>5.13</feature>
              <feature>5.14</feature>
              <feature>5.13</feature>
              <feature>5.13</feature>
              <feature>5.13</feature>
              <feature>4.13</feature>
              <feature>5.17</feature>
            </features>
              <calculated mass_charge="3.4"/>
            <peptide>Z.RLKNGN.K</peptide>
            <protein id="genA"/>
            <protein id="genB"/>
          </peptide_spectrum_match>
    </frag_spectrum_scan>
    <frag_spectrum_scan num="2">
      <observed mass_charge="2.1"/>
      <ion_current val="5.6"></ion_current>
          <peptide_spectrum_match charge="12" type="target" id="peptide_spectrum_match1">
            <features>
              <feature>2.13</feature>
              <feature>2.14</feature>
              <feature>3.13</feature>
              <feature>2.13</feature>
              <feature>2.13</feature>
              <feature>4.13</feature>
              <feature>8.17</feature>
            </features>
              <calculated mass_charge="3.4"/>
              <peptide>N.QRLKNGN.K</peptide>
            <protein id="genA"/>
            <protein id="genB"/>
          </peptide_spectrum_match>
          <peptide_spectrum_match charge="14" type="decoy"  id="peptide_spectrum_match2">
            <features>
              <feature>5.13</feature>
              <feature>5.14</feature>
              <feature>5.13</feature>
              <feature>5.13</feature>
              <feature>5.13</feature>
              <feature>4.13</feature>
              <feature>5.17</feature>
            </features>
              <calculated mass_charge="3.4"/>
            <peptide>Z.RLKNGN.K</peptide>
            <protein id="genA"/>
            <protein id="genB"/>
          </peptide_spectrum_match>
    </frag_spectrum_scan>
  </experiment>

