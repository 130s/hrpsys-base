from omniORB import CORBA, any
import CosNaming

import RTC, OpenRTM, SDOPackage, RTM

import sys
import string, math, socket

##
# \brief root naming context
#
rootnc = None

##
# \brief wrapper class of RT component
#
class RTcomponent:
	##
	# \brief constructor
	# \param self this object
	# \param ref IOR of RT component
	#
	def __init__(self, ref):
		self.ref = ref
		self.ec = ref.get_owned_contexts()[0]
	
	##
	# \brief get IOR of port
	# \param self this object
	# \param name name of the port
	# \return IOR of the port
	def port(self, name):
		return findPort(self.ref, name)

	##
	# \brief get IOR of service
	# \param self this object
	# \param name name of service
	# \return IOR of the service
	def service(self, name):
		return findService(self.ref, name)

	##
	# \brief update default configuration set
	# \param self this object
	# \param nvlist list of pairs of name and value
	def setConfiguration(self, nvlist):
		setConfiguration(self.ref, nvlist)

        ##
	# \brief update value of the default configuration set
	# \param self this object	
	# \param name name of the property
	# \param value new value of the property
	def setProperty(self, name, value):
		self.setConfiguration([[name, value]])

        ##
	# \brief get value of the property in the default configuration set
	# \param self this object	
	# \param name name of the property
        # \return value of the property or None if the property is not found
	def getProperty(self, name):
		cfg = self.ref.get_configuration()
		cfgsets = cfg.get_configuration_sets()
		if len(cfgsets) == 0:
			print "configuration set is not found"
			return None
		cfgset = cfgsets[0]
		for d in cfgset.configuration_data:
			if d.name == name:
				return d.value.extract_string()
		return None		

	##
	# \brief activate this component
	# \param self this object
	def start(self):
		if self.ec != None:
			self.ec.activate_component(self.ref)

	##
	# \brief deactivate this component
	# \param self this object
	def stop(self):
		if self.ec != None:
			self.ec.deactivate_component(self.ref)

	##
	# \brief get life cycle state of the main execution context
        # \return one of LifeCycleState value or None if the main execution context is not set
	def getLifeCycleState(self):
		if self.ec != None:
			return self.ec.get_component_state(self.ref)
		else:
			return None

	##
	# \brief check the main execution context is active or not
        # \retval 1 this component is active
        # \retval 0 this component is not active
        def isActive(self):
		return LifeCycleState.ACTIVE_STATE == self.getLifeCycleState()

	##
	# \brief get instance name
	# \return instance name
	def name(self):
		cprof = self.ref.get_component_profile()
		return cprof.instance_name

##
# \brief wrapper class of RTCmanager
#
class RTCmanager:
	##
	# \brief constructor
	# \param self this object
	# \param ref IOR of RTCmanager
	#
	def __init__(self, ref):
		self.ref = ref
	
	##
	# \brief load RT component factory
	# \param self this object
	# \param basename common part of path of the shared library and the initialize function. path is generated by basename+".so" and the initialize function is generated by basename+"Init".
	#
	def load(self, basename):
		path = basename+".so"
		initfunc = basename+"Init"
		try:
			self.ref.load_module(path, initfunc)
		except:
			print "failed to load",path

	##
	# \brief create an instance of RT component
	# \param self this object
	# \param module name of RT component factory
	# \return an object of RTcomponent
	def create(self, module):
		ref = self.ref.create_component(module)
		if ref == None:
			return None
		else:
			return RTcomponent(ref)

	##
	# \brief get list of factory names
        # \return list of factory names
	def get_factory_names(self):
		fs = []
		fps = self.ref.get_factory_profiles()
		for afp in fps:
			for p in afp.properties:
				if p.name == "implementation_id":
					fs.append(p.value.extract_string())
		return fs

	##
	# \brief get list of components
        # \return list of components
	def get_components(self):
		cs = []
		crefs = self.ref.get_components()
		for cref in crefs:
			c = RTcomponent(cref)
			cs.append(c)
		return cs	

##
# \brief unbind an object reference 
# \param name name of the object
# \param kind kind of the object
#
def unbindObject(name, kind):
	nc = NameComponent(name, kind)
	path = [nc]
	rootnc.unbind(path)
	return None

##
# \brief initialize ORB 
#
def initCORBA():
	global rootnc, orb
	orb = CORBA.ORB_init(sys.argv, CORBA.ORB_ID)

	nameserver = orb.resolve_initial_references("NameService");
	rootnc = nameserver._narrow(CosNaming.NamingContext)
	return None

##
# \brief get root naming context 
# \param corbaloc location of NamingService
# \return root naming context
#
def getRootNamingContext(corbaloc):
	props = System.getProperties()
	
	args = ["-ORBInitRef", corbaloc]
	orb = ORB.init(args, props)

	nameserver = orb.resolve_initial_references("NameService");
	return NamingContextHelper.narrow(nameserver);

##
# \brief get IOR of the object
# \param name name of the object
# \param kind kind of the object
# \param rnc root naming context. If it is not specified, global variable rootnc is used
# \return IOR of the object
# 
def findObject(name, kind, rnc=None):
	nc = CosNaming.NameComponent(name,kind)
	path = [nc]
	if not rnc:
		rnc = rootnc
	return rnc.resolve(path)

##
# \brief get RTCmanager
# \param hostname hostname where rtcd is running
# \param rnc root naming context. If it is not specified, global variable rootnc is used
# \return an object of RTCmanager
#
def findRTCmanager(hostname=socket.gethostname(), rnc=None):
	try:
		cxt = findObject(hostname, "host_cxt", rnc)
		obj = findObject("manager","mgr",cxt)
		return RTCmanager(obj._narrow(RTM.Manager))
	except:
		print "exception in findRTCmanager("+hostname+")"

##
# \brief get RT component
# \param name name of the RT component
# \param rnc root naming context. If it is not specified, global variable rootnc is used
# \return an object of RTcomponent
#
def findRTC(name, rnc=None):
	try:
		obj = findObject(name, "rtc", rnc)
		return RTcomponent(obj._narrow(RTC.RTObject))
	except:
		print "exception in findRTC("+name+")"

##
# \brief get a port of RT component
# \param rtc an object of RTcomponent
# \param name name of the port
# \return IOR of the port if the port is found, None otherwise
#
def findPort(rtc, name):
	ports = rtc.get_ports()
	cprof = rtc.get_component_profile()
	portname = cprof.instance_name+"."+name
	for p in ports:
		prof = p.get_port_profile()
		if prof.name == portname:
			return p
	return None 

##
# \brief set up execution context of the first RTC so that RTCs are executed sequentially
# \param rtcs sequence of RTCs
#
def serializeComponents(rtcs):
	if len(rtcs) == 0:
		return
	ec = rtcs[0].ec
	for rtc in rtcs[1:]:
		ec.add_component(rtc.ref)
		rtc.ec = ec

##
# \brief check two ports are connected or not
# \retval True connected
# \retval False not connected
def isConnected(outP, inP):
	op = outP.get_port_profile()
	for con_prof in op.connector_profiles:
		ports = con_prof.ports
		if outP == ports[0] and inP == ports[1]:
			return True
	return False
	
##
# \brief connect ports
# \param outP IOR of outPort 
# \param inP IOR of inPort 
#
def connectPorts(outP, inP):
	if isConnected(outP, inP) == True:
		return
	nv1 = SDOPackage.NameValue("dataport.interface_type", 
				   any.to_any("corba_cdr"))
	nv2 = SDOPackage.NameValue("dataport.dataflow_type", 
				   any.to_any("Push"))
	nv3 = SDOPackage.NameValue("dataport.subscription_type", 
				   any.to_any("flush"))
	con_prof = RTC.ConnectorProfile("connector0", "", [outP, inP], 
					[nv1, nv2, nv3])
	inP.connect(con_prof)

##
# \brief get a service of RT component
# \param rtc IOR of RT component
# \param svcname name of the service
# \return IOR of the service
#
def findService(rtc, svcname):
	prof = rtc.get_component_profile()
	print "RTC name:",prof.instance_name
	port_prof = prof.port_profiles
	port = None
	for pp in port_prof:
		print "name:",pp.name
		ifs = pp.interfaces
		for aif in ifs:
			print "IF name:",aif.instance_name
			print "IF type:",aif.type_name
			if aif.instance_name == svcname:
				port = pp.port_ref
	con_prof = RTC.ConnectorProfile("noname","",[port],[])
	ret, con_prof = port.connect(con_prof)
	ior = any.from_any(con_prof.properties[0].value)
	return orb.string_to_object(ior)

##
# \brief update default configuration set
# \param rtc IOR of RT component
# \param nvlist list of pairs of name and value
#
def setConfiguration(rtc, nvlist):
	cfg = rtc.get_configuration()
	cfgsets = cfg.get_configuration_sets()
	if len(cfgsets) == 0:
		print "configuration set is not found"
		return
	cfgset = cfgsets[0]
	for nv in nvlist:
		name = nv[0]
		value = nv[1]
		for d in cfgset.configuration_data:
			if d.name == name:
				d.value.insert_string(value)
				cfg.set_configuration_set_values(cfgset)
				break;
	cfg.activate_configuration_set('default')

initCORBA()
