// -*- mode: c++; indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4; -*-
#ifndef __KALMANFILTER_SERVICE_H__
#define __KALMANFILTER_SERVICE_H__

#include "KalmanFilterService.hh"

class KalmanFilter;

class KalmanFilterService_impl
	: public virtual POA_OpenHRP::KalmanFilterService,
	  public virtual PortableServer::RefCountServantBase
{
public:
	/**
	   \brief constructor
	*/
	KalmanFilterService_impl();

	/**
	   \brief destructor
	*/
	virtual ~KalmanFilterService_impl();

	bool SetKalmanFilterParam(double Q_angle, double Q_rate, double R_angle);

	void kalman(KalmanFilter *i_kalman);

private:
	KalmanFilter *m_kalman;
};

#endif
