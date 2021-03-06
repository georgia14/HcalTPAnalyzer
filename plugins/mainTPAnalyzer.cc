// -*- C++ -*-
//
// Package:    UserCode/HcalTPAnalyzer
// Class:      mainTPAnalyzer
// 
/**\class mainTPAnalyzer mainTPAnalyzer.cc UserCode/HcalTPAnalyzer/plugins/mainTPAnalyzer.cc

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  Georgia Karapostoli
//         Created:  Sun, 24 Sep 2017 18:02:34 GMT
//
//


// system include files
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "CalibFormats/CaloTPG/interface/CaloTPGTranscoder.h"
#include "CalibFormats/CaloTPG/interface/CaloTPGRecord.h"
#include "CalibFormats/HcalObjects/interface/HcalDbRecord.h"
#include "CalibFormats/HcalObjects/interface/HcalDbService.h"

#include "CondFormats/DataRecord/interface/HcalChannelQualityRcd.h"
#include "CondFormats/DataRecord/interface/L1CaloGeometryRecord.h"
#include "CondFormats/HcalObjects/interface/HcalChannelQuality.h"
#include "CondFormats/L1TObjects/interface/L1CaloGeometry.h"

#include "CondFormats/L1TObjects/interface/L1RCTParameters.h"
#include "CondFormats/DataRecord/interface/L1RCTParametersRcd.h"
#include "CondFormats/L1TObjects/interface/L1CaloHcalScale.h"
#include "CondFormats/DataRecord/interface/L1CaloHcalScaleRcd.h"

#include "DataFormats/Common/interface/SortedCollection.h"
#include "DataFormats/CaloTowers/interface/CaloTower.h"
#include "DataFormats/HcalDigi/interface/HcalDigiCollections.h"
#include "DataFormats/HcalDigi/interface/HcalTriggerPrimitiveDigi.h"
#include "DataFormats/HcalDetId/interface/HcalTrigTowerDetId.h"
#include "DataFormats/HcalDetId/interface/HcalDetId.h"
#include "DataFormats/L1CaloTrigger/interface/L1CaloCollections.h"

#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/HcalTowerAlgo/interface/HcalGeometry.h"
#include "Geometry/HcalTowerAlgo/interface/HcalTrigTowerGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"

#include "FWCore/Common/interface/TriggerNames.h"   
#include "DataFormats/Common/interface/TriggerResults.h"

#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"

#include "UserCode/HcalTPAnalyzer/interface/MacroUtils.h"
#include "UserCode/HcalTPAnalyzer/interface/DataEvtSummaryHandler.h"

#include "TSystem.h"
#include "TROOT.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TString.h"
#include "TTree.h"

using namespace utils;
//
// class declaration
//

// If the analyzer does not use TFileService, please remove
// the template argument to the base class so the class inherits
// from  edm::one::EDAnalyzer<> and also remove the line from
// constructor "usesResource("TFileService");"
// This will improve performance in multithreaded jobs.

class mainTPAnalyzer : public edm::one::EDAnalyzer<edm::one::SharedResources>  {
   public:
      explicit mainTPAnalyzer(const edm::ParameterSet&);
      ~mainTPAnalyzer();

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);


   private:
      virtual void beginJob() override;
      virtual void analyze(const edm::Event&, const edm::EventSetup&) override;
      virtual void endJob() override;
  
      // ----------member data ---------------------------
  edm::InputTag digis_;

  bool isData_;
  bool verbose_;
  bool doReco_;
  
  unsigned int maxVtx_;  
  std::vector<edm::InputTag> vtxToken_;

  double threshold_;

  edm::EDGetTokenT<edm::TriggerResults> triggerBits_;
  
  DataEvtSummaryHandler summaryHandler_;

  std::array<int, 10> tp_adc_;
  // std::array<int, 10> tp_adc_emul_;

  //  static const int FGCOUNT = 7;
  std::array<int, FGCOUNT> tp_fg_;
  //  std::array<int, FGCOUNT> tp_fg_emul_;
  
};

//
// constants, enums and typedefs
//

//
// static data member definitions
//

//
// constructors and destructor
//
mainTPAnalyzer::mainTPAnalyzer(const edm::ParameterSet& iConfig):
  digis_( iConfig.getParameter<edm::InputTag>("triggerPrimitives") ),
  isData_( iConfig.getParameter<bool>("isData") ),
  verbose_( iConfig.getParameter<bool>("verbose") ),
  doReco_( iConfig.getParameter<bool>("doReco") ),
  maxVtx_( iConfig.getParameter<unsigned int>("maxVtx") ),
  vtxToken_( iConfig.getUntrackedParameter<std::vector<edm::InputTag>>("vtxToken") ),
  threshold_( iConfig.getUntrackedParameter<double>("threshold", 0.) ),
  triggerBits_( consumes<edm::TriggerResults>(iConfig.getParameter<edm::InputTag>("bits")) )

{
   //now do what ever initialization is needed
  edm::Service<TFileService> fs;

  summaryHandler_.initTree( fs->make<TTree>("tpdata","Event Summary") );
  TFileDirectory baseDir=fs->mkdir(iConfig.getParameter<std::string>("dtag"));
  
  consumes<HcalTrigPrimDigiCollection>(digis_);
  if (doReco_) consumes<reco::VertexCollection>(vtxToken_[0]);

   //usesResource("TFileService");

}


mainTPAnalyzer::~mainTPAnalyzer()
{
 
   // do anything here that needs to be done at desctruction time
   // (e.g. close files, deallocate resources etc.)

}

namespace std {
   template<> struct hash<HcalTrigTowerDetId> {
      size_t operator()(const HcalTrigTowerDetId& id) const {
         return hash<int>()(id);
      }
   };
}


//
// member functions
//

// ------------ method called for each event  ------------
void
mainTPAnalyzer::analyze(const edm::Event& event, const edm::EventSetup& setup)
{
   using namespace edm;
   
   summaryHandler_.resetStruct();
   //event summary to be filled
   DataEvtSummary_t &ev=summaryHandler_.getEvent();

   ev.run = event.eventAuxiliary().run() ;
   ev.lumi = event.eventAuxiliary().luminosityBlock() ;
   ev.bx = event.eventAuxiliary().bunchCrossing(); 
   ev.event = event.eventAuxiliary().event() ;

   if ( verbose_ ) { printf("\n\n ================= Run %u , lumi %u , event %lld\n\n", ev.run, ev.lumi, ev.event ) ; }

   //apply trigger
   edm::Handle<edm::TriggerResults> triggerBits;
   event.getByToken(triggerBits_, triggerBits);
   
   edm::TriggerResultsByName tr(nullptr,nullptr);
   tr = event.triggerResultsByName(*triggerBits); //"HLT");
   if(!tr.isValid() )return;

   bool zerobiasTrigger(true);
   zerobiasTrigger = utils::cmssw::passTriggerPatterns(tr, "HLT_ZeroBias_v*");

   ev.hasTrigger = ( zerobiasTrigger );
   //  if (!ev.hasTrigger) return;
   
   if (doReco_) {
     ev.nvtx=0;
     
     // get vertices
     edm::Handle<reco::VertexCollection> vertices; 
     event.getByLabel(vtxToken_[0], vertices); 
     if (vertices.isValid()) {
       unsigned int nVtx_=0; 
       for(reco::VertexCollection::const_iterator it=vertices->begin();
	   it!=vertices->end() && nVtx_ < maxVtx_; 
	   ++it) {
	 if (!it->isFake()) { nVtx_++; }
       }
       ev.nvtx=nVtx_;
     }   
   } // doReco  

   Handle<HcalTrigPrimDigiCollection> digis;
   if (!event.getByLabel(digis_, digis)) {
      LogError("AnalyzeTP") <<
         "Can't find hcal trigger primitive digi collection with tag '" <<
         digis_ << "'" << std::endl;
      return;
   }
   
   ESHandle<CaloTPGTranscoder> decoder;
   setup.get<CaloTPGRecord>().get(decoder);

   std::unordered_set<HcalTrigTowerDetId> ids;
   typedef std::unordered_map<HcalTrigTowerDetId, HcalTriggerPrimitiveDigi> digi_map;
   digi_map ds;

   // Make association of a TP digi with a TT Id.
   for (const auto& digi: *digis) {
      ids.insert(digi.id());
      ds[digi.id()] = digi;
   }

   
   ev.ntp=0;
   for (const auto& id: ids) { // loop over TT vector
     if (id.version() == 1 and abs(id.ieta()) >= 40 and id.iphi() % 4 == 1)
       continue;
     
     ev.tp_ieta_[ev.ntp] = id.ieta();
     ev.tp_iphi_[ev.ntp] = id.iphi();
     ev.tp_depth_[ev.ntp] = id.depth();
     ev.tp_version_[ev.ntp] = id.version();

     digi_map::const_iterator digi;

     if ((digi = ds.find(id)) != ds.end()) { // Find the associated TT (id) in the digi_map

       ev.tp_soi_[ev.ntp] = digi->second.SOI_compressedEt();
       ev.tp_npresamples_[ev.ntp] = digi->second.presamples();
       ev.tp_unzs_[ev.ntp] = digi->second.zsUnsuppressed();
       
       ev.tp_et_[ev.ntp] = decoder->hcaletValue(id, digi->second.t0());

       for (unsigned int i = 0; i < tp_fg_.size(); ++i) {
	 ev.tp_fg_[ev.ntp][i] = digi->second.t0().fineGrain(i);
       }
       for (unsigned int i = 0; i < tp_adc_.size(); ++i) {
	 ev.tp_adc_[ev.ntp][i] = digi->second[i].compressedEt();
       }

     } else {
       ev.tp_soi_[ev.ntp] = 0;
       ev.tp_npresamples_[ev.ntp] = 0;
       ev.tp_unzs_[ev.ntp] = false;
       ev.tp_et_[ev.ntp] = 0;

       for (unsigned int i = 0; i < tp_fg_.size(); ++i) {
	 ev.tp_fg_[ev.ntp][i] = 0;
       }
       for (unsigned int i = 0; i < tp_adc_.size(); ++i) {
	 ev.tp_adc_[ev.ntp][i] = 0;
       }

     }
     
     // auto new_id(id);
     // if (swap_iphi_ and id.version() == 1 and id.ieta() > 28 and id.ieta() < 40) {
     //   if (id.iphi() % 4 == 1)
     // 	 new_id = HcalTrigTowerDetId(id.ieta(), (id.iphi() + 70) % 72, id.depth(), id.version());
     //   else
     // 	 new_id = HcalTrigTowerDetId(id.ieta(), (id.iphi() + 2) % 72 , id.depth(), id.version());
     // }
     // if ((digi = eds.find(new_id)) != eds.end()) {
     //   tp_soi_emul_ = digi->second.SOI_compressedEt();
     //   tp_npresamples_emul_ = digi->second.presamples();
     //   tp_et_emul_ = decoder->hcaletValue(id, digi->second.t0());
     //   for (unsigned int i = 0; i < tp_fg_emul_.size(); ++i)
     // 	 tp_fg_emul_[i] = digi->second.t0().fineGrain(i);
     //   for (unsigned int i = 0; i < tp_adc_emul_.size(); ++i)
     // 	 tp_adc_emul_[i] = digi->second[i].compressedEt();
     // } else {
     //   tp_soi_emul_ = 0;
     //   tp_npresamples_emul_ = 0;
     //   tp_et_emul_ = 0;
     //   for (unsigned int i = 0; i < tp_fg_emul_.size(); ++i)
     // 	 tp_fg_emul_[i] = 0;
     //   for (unsigned int i = 0; i < tp_adc_emul_.size(); ++i)
     // 	 tp_adc_emul_[i] = 0;
     // }
     ev.ntp++;
     if (ev.ntp>MAXTPS) { printf("Number of TPs exceeds MAX = %3d ! System will exit",MAXTPS); gSystem->Exit(-1); }
   }

   
   // Fill Tree
   summaryHandler_.fillTree();
}


// ------------ method called once each job just before starting event loop  ------------
void 
mainTPAnalyzer::beginJob()
{
}

// ------------ method called once each job just after ending the event loop  ------------
void 
mainTPAnalyzer::endJob() 
{
}

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void
mainTPAnalyzer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(mainTPAnalyzer);
