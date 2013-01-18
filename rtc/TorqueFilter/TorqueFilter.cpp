// -*- C++ -*-
/*!
 * @file  TorqueFilter.cpp
 * @brief null component
 * $Date$
 *
 * $Id$
 */

#include "TorqueFilter.h"
#include <rtm/CorbaNaming.h>
#include <hrpModel/ModelLoaderUtil.h>
#include <hrpUtil/MatrixSolvers.h>

#define DEBUGP ((m_debugLevel==1 && loop%200==0) || m_debugLevel > 1 )

// Module specification
// <rtc-template block="module_spec">
static const char* torquefilter_spec[] =
{
  "implementation_id", "TorqueFilter",
  "type_name",         "TorqueFilter",
  "description",       "null component",
  "version",           "1.0",
  "vendor",            "AIST",
  "category",          "example",
  "activity_type",     "DataFlowComponent",
  "max_instance",      "10",
  "language",          "C++",
  "lang_type",         "compile",
  // Configuration variables
  "conf.default.debugLevel", "0",
  ""
};

// without specialization, stringTo only convert 0/1 in bool
// namespace coil{
//   template <>
//   bool stringTo(bool& val, const char* str)
//   {
//     if (str == 0) { return false; }
//     std::stringstream s;
//     if ((s << str).fail()) { return false; }
//     if ((s >> std::boolalpha >> val).fail()) { return false; }
//     return true;
//   }
// }

// </rtc-template>

TorqueFilter::TorqueFilter(RTC::Manager* manager)
  : RTC::DataFlowComponentBase(manager),
    // <rtc-template block="initializer">
    m_qCurrentIn("qCurrent", m_qCurrent),
    m_tauInIn("tauIn", m_tauIn),
    m_tauOutOut("tauOut", m_tauOut),
    // </rtc-template>
    m_debugLevel(0),
    m_is_gravity_compensation(false)
{
}

TorqueFilter::~TorqueFilter()
{
}

RTC::ReturnCode_t TorqueFilter::onInitialize()
{
  std::cout << m_profile.instance_name << ": onInitialize()" << std::endl;
  // <rtc-template block="bind_config">
  // Bind variables and configuration variable
  bindParameter("debugLevel", m_debugLevel, "0");
  
  // </rtc-template>

  // Registration: InPort/OutPort/Service
  // <rtc-template block="registration">
  // Set InPort buffers
  addInPort("qCurrent", m_qCurrentIn);
  addInPort("tauIn", m_tauInIn);

  // Set OutPort buffer
  addOutPort("tauOut", m_tauOutOut);
  
  // Set service provider to Ports
  
  // Set service consumers to Ports
  
  // Set CORBA Service Ports
  
  // </rtc-template>

  RTC::Properties& prop = getProperties();
  coil::stringTo(m_dt, prop["dt"].c_str());

  m_robot = hrp::BodyPtr(new hrp::Body());

  RTC::Manager& rtcManager = RTC::Manager::instance();
  std::string nameServer = rtcManager.getConfig()["corba.nameservers"];
  int comPos = nameServer.find(",");
  if (comPos < 0){
    comPos = nameServer.length();
  }
  nameServer = nameServer.substr(0, comPos);
  RTC::CorbaNaming naming(rtcManager.getORB(), nameServer.c_str());
  if (!loadBodyFromModelLoader(m_robot, prop["model"].c_str(),
                               CosNaming::NamingContext::_duplicate(naming.getRootContext())
        )){
    std::cerr << "failed to load model[" << prop["model"] << "]"
              << std::endl;
  }

  // init outport
  m_tauOut.data.length(m_robot->numJoints());

  // set gravity compensation flag
  coil::stringTo(m_is_gravity_compensation, prop["gravity_compensation"].c_str());
  std::cerr << "gravity compensation: " << m_is_gravity_compensation << std::endl;
  
  // set torque offset
  // offset = -(gear torque in neutral pose)
  m_torque_offset.resize(m_robot->numJoints());
  coil::vstring torque_offset = coil::split(prop["torque_offset"], ",");
  assert(m_torque_offset.size() == torque_offset.size());
  for(int i = 0; i < m_robot->numJoints(); i++){
    coil::stringTo(m_torque_offset[i], torque_offset[i].c_str());
    std::cerr << "offset[" << m_robot->joint(i)->name << "]: " << m_torque_offset[i] << std::endl;
  }
  
  // make filter
  // filter_dim, fb_coeffs[0], ..., fb_coeffs[filter_dim], ff_coeffs[0], ..., ff_coeffs[filter_dim]
  coil::vstring torque_filter_params = coil::split(prop["torque_filter_params"], ","); // filter values
  int filter_dim;
  coil::stringTo(filter_dim, torque_filter_params[0].c_str());
  std::cerr << "filter dim: " << filter_dim << std::endl;
  // ex) 2dim butterworth filter sampling = 200[hz] cutoff = 5[hz]
  // octave$ [a, b] = butter(2, 5/200)
  // fb_coeffs[0] = 1.00000; <- b0
  // fb_coeffs[1] = 1.88903; <- -b1
  // fb_coeffs[2] = -0.89487; <- -b2
  // ff_coeffs[0] = 0.0014603; <- a0
  // ff_coeffs[1] = 0.0029206; <- a1
  // ff_coeffs[2] = 0.0014603; <- a2
  std::vector<double> fb_coeffs(filter_dim + 1);
  std::vector<double> ff_coeffs(filter_dim + 1);
  for(int i = 0; i < filter_dim + 1; i++){
    coil::stringTo(fb_coeffs[i], torque_filter_params[i + 1].c_str());
    coil::stringTo(ff_coeffs[i], torque_filter_params[i + (filter_dim + 2)].c_str());    
    std::cerr << "fb[" << i << "]: " << fb_coeffs[i] << std::endl;
    std::cerr << "ff[" << i << "]: " << ff_coeffs[i] << std::endl;
  }
  for(int i = 0; i < m_robot->numJoints(); i++){
    m_filters.push_back(IIRFilter(filter_dim, fb_coeffs, ff_coeffs));
  }
  
  return RTC::RTC_OK;
}



/*
  RTC::ReturnCode_t TorqueFilter::onFinalize()
  {
  return RTC::RTC_OK;
  }
*/

/*
  RTC::ReturnCode_t TorqueFilter::onStartup(RTC::UniqueId ec_id)
  {
  return RTC::RTC_OK;
  }
*/

/*
  RTC::ReturnCode_t TorqueFilter::onShutdown(RTC::UniqueId ec_id)
  {
  return RTC::RTC_OK;
  }
*/

RTC::ReturnCode_t TorqueFilter::onActivated(RTC::UniqueId ec_id)
{
  std::cout << m_profile.instance_name<< ": onActivated(" << ec_id << ")" << std::endl;
  return RTC::RTC_OK;
}

RTC::ReturnCode_t TorqueFilter::onDeactivated(RTC::UniqueId ec_id)
{
  std::cout << m_profile.instance_name<< ": onDeactivated(" << ec_id << ")" << std::endl;
  return RTC::RTC_OK;
}

RTC::ReturnCode_t TorqueFilter::onExecute(RTC::UniqueId ec_id)
{
  // std::cout << m_profile.instance_name<< ": onExecute(" << ec_id << ")" << std::endl;
  static int loop = 0;
  loop ++;

  coil::TimeValue coiltm(coil::gettimeofday());
  RTC::Time tm;
  tm.sec = coiltm.sec();
  tm.nsec = coiltm.usec()*1000;

  if (m_qCurrentIn.isNew()) {
    m_qCurrentIn.read();
  }
  if (m_tauInIn.isNew()) {
    m_tauInIn.read();
  }

  if ( m_qCurrent.data.length() ==  m_robot->numJoints() &&
       m_tauIn.data.length() ==  m_robot->numJoints() ) {
    // reference robot model
    for ( int i = 0; i < m_robot->numJoints(); i++ ){
      m_robot->joint(i)->q = m_qCurrent.data[i];
    }
    m_robot->calcForwardKinematics();
    m_robot->calcCM();
    m_robot->rootLink()->calcSubMassCM();


    int num_joints = m_robot->numJoints();
    hrp::dvector torque(num_joints);
     
    // calc gravity compensation of each joints
    hrp::Vector3 g(0, 0, 9.8);
    hrp::dvector g_joint_torque(num_joints);
    for (int i = 0; i < num_joints; i++) {
      // subm*g x (submwc/subm - p) . R*a
      g_joint_torque[i] = (m_robot->joint(i)->subm*g).cross(m_robot->joint(i)->submwc / m_robot->joint(i)->subm - m_robot->joint(i)->p).dot(m_robot->joint(i)->R * m_robot->joint(i)->a);
    }
      
    if ( DEBUGP ) {
      std::cerr << "raw torque: ";
      for (int i = 0; i < num_joints; i++) {
        std::cerr << " " << m_tauIn.data[i] ;
      }
      std::cerr << std::endl;
      std::cerr << "  gravity compensation: ";
      for (int i = 0; i < num_joints; i++) {
        std::cerr << " " << g_joint_torque[i];
      }
      std::cerr << std::endl;
    }

    for (int i = 0; i < num_joints; i++) {
      // torque calculation from electric current
      // torque[j] = m_tauIn.data[path->joint(j)->jointId] - joint_torque(j);
      // torque[j] = m_filters[path->joint(j)->jointId].executeFilter(m_tauIn.data[path->joint(j)->jointId]) - joint_torque(j); // use filtered tau
      torque[i] = m_filters[i].executeFilter(m_tauIn.data[i]) - m_torque_offset[i];

      // torque calclation from error of joint angle
      // if ( m_error_to_torque_gain[path->joint(j)->jointId] == 0.0
      //      || fabs(joint_error(j)) < m_error_dead_zone[path->joint(j)->jointId] ) {
      //   torque[j] = 0;
      // } else {
      //   torque[j] = error2torque(j, fabs(m_error_dead_zone[path->joint(j)->jointId]));
      // }

      // set output
      // gravity compensation
      if(m_is_gravity_compensation){
        m_tauOut.data[i] = torque[i] + g_joint_torque[i];
      } else {
        m_tauOut.data[i] = torque[i];
      }
    }
      
    if ( DEBUGP ) {
      std::cerr << "  torque  : ";
      for (int i = 0; i < num_joints; i++) {
        std::cerr << " " << torque[i];
      }
      std::cerr << std::endl;
    }
    
    m_tauOutOut.write();
  }
  return RTC::RTC_OK;
}

/*
  RTC::ReturnCode_t TorqueFilter::onAborting(RTC::UniqueId ec_id)
  {
  return RTC::RTC_OK;
  }
*/

/*
  RTC::ReturnCode_t TorqueFilter::onError(RTC::UniqueId ec_id)
  {
  return RTC::RTC_OK;
  }
*/

/*
  RTC::ReturnCode_t TorqueFilter::onReset(RTC::UniqueId ec_id)
  {
  return RTC::RTC_OK;
  }
*/

/*
  RTC::ReturnCode_t TorqueFilter::onStateUpdate(RTC::UniqueId ec_id)
  {
  return RTC::RTC_OK;
  }
*/

/*
  RTC::ReturnCode_t TorqueFilter::onRateChanged(RTC::UniqueId ec_id)
  {
  return RTC::RTC_OK;
  }
*/



extern "C"
{

  void TorqueFilterInit(RTC::Manager* manager)
  {
    RTC::Properties profile(torquefilter_spec);
    manager->registerFactory(profile,
                             RTC::Create<TorqueFilter>,
                             RTC::Delete<TorqueFilter>);
  }

};


