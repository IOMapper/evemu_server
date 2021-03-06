/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2011 The EVEmu Team
    For the latest information visit http://evemu.org
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:        Zhur
*/

#include "eve-server.h"

#include "PyBoundObject.h"
#include "PyServiceCD.h"
#include "ship/InsuranceService.h"

/* TODO:
 *   - handle ship-destroyed event (remove insurance, pay out value, etc...)
 *   - use history market value from marketProxy!
 */

class InsuranceBound
: public PyBoundObject
{
public:
    PyCallable_Make_Dispatcher(InsuranceBound)

    InsuranceBound(PyServiceMgr *mgr, ShipDB* db)
    : PyBoundObject(mgr),
      m_db(db),
      m_dispatch(new Dispatcher(this))
    {
        _SetCallDispatcher(m_dispatch);

        PyCallable_REG_CALL(InsuranceBound, GetContracts)
        PyCallable_REG_CALL(InsuranceBound, GetInsurancePrice)
        PyCallable_REG_CALL(InsuranceBound, InsureShip)

        m_strBoundObjectName = "InsuranceBound";
    }

    virtual ~InsuranceBound() { delete m_dispatch; }
    virtual void Release() {
        //I hate this statement
        delete this;
    }

    PyCallable_DECL_CALL(GetContracts)
    PyCallable_DECL_CALL(GetInsurancePrice)
    PyCallable_DECL_CALL(InsureShip)

protected:
    ShipDB* m_db;
    Dispatcher *const m_dispatch;
};

PyCallable_Make_InnerDispatcher(InsuranceService)

InsuranceService::InsuranceService(PyServiceMgr *mgr)
: PyService(mgr, "insuranceSvc"),
  m_dispatch(new Dispatcher(this))
{
    _SetCallDispatcher(m_dispatch);

    PyCallable_REG_CALL(InsuranceService, GetContractForShip)
    PyCallable_REG_CALL(InsuranceService, GetInsurancePrice)
}

InsuranceService::~InsuranceService() {
    delete m_dispatch;
}

PyBoundObject* InsuranceService::_CreateBoundObject( Client* c, const PyRep* bind_args )
{
    _log( CLIENT__MESSAGE, "InsuranceService bind request for:" );
    bind_args->Dump( CLIENT__MESSAGE, "    " );

    return new InsuranceBound( m_manager, &m_db );
}

PyResult InsuranceService::Handle_GetInsurancePrice( PyCallArgs& call )
{
    sLog.Debug("InsuranceService", "Called GetInsurancePrice stub" );

	call.Dump(DEBUG__DEBUG);

    return new PyFloat(0.0);
}

PyResult InsuranceBound::Handle_GetInsurancePrice(PyCallArgs& call)
{
	/*
	Call Arguments:
		Tuple: 1 elements
			[ 0] Integer field: 606		// typeID
	Call Named Arguments:
		Argument 'machoVersion':
			Integer field: 1
	*/
	uint32 typeID = call.tuple->GetItem(0)->AsInt()->value();

	const ItemType *type = m_manager->item_factory.GetType(typeID);
	if(!type)
		return new PyNone;

	return new PyFloat(type->basePrice());
}

PyResult InsuranceService::Handle_GetContractForShip( PyCallArgs& call )
{
	/*
	Call Arguments:
		Tuple: 1 elements
			[ 0] Integer field: 140000078	// shipID
	Call Named Arguments:
		Argument 'machoVersion':
			Integer field: 1
	*/

	uint32 shipID = call.tuple->GetItem(0)->AsInt()->value();

	return m_db.GetInsuranceInfoByShipID(shipID);
}

PyResult InsuranceBound::Handle_GetContracts( PyCallArgs& call )
{
    //sLog.Debug("InsuranceBound", "Called GetContracts WORK IN PROGRESS!" );

	/*
	Call Arguments:
		Tuple: Empty
	*/
	//call.Dump(DEBUG__DEBUG);

	uint32 ownerID = call.client->GetCharacterID();

	return m_db->GetInsuranceContractsByOwnerID(ownerID);
}

PyResult InsuranceBound::Handle_InsureShip( PyCallArgs& call )
{
	/*
	Call Arguments:
	    Tuple: 3 elements
			[ 0] Integer field: 140000078			// shipID / entityID
			[ 1] Real field: 1125.000000			// payment in ISK
			[ 2] Integer field: 0					// ?
	Call Named Arguments:
		Argument 'machoVersion':
			Integer field: 1
	*/
//	call.Dump(DEBUG__DEBUG);

	/* INSURANCE FRACTION TABLE:
			Label    Fraction  Pay
			-------- --------- ------
			Platin   1.0       0.3
			Gold     0.9       0.25
			Silver   0.8       0.2
			Bronze   0.7       0.15
			Standard 0.6       0.1
			Basis    0.5       0.05
	*/

	uint32 shipID = call.tuple->GetItem(0)->AsInt()->value();
	double payment = call.tuple->GetItem(1)->AsFloat()->value();

    ShipRef ship = m_manager->item_factory.GetShip(shipID);

	// calculate the fraction value
    double shipValue = ship->type().basePrice();
    double paymentFraction = payment/shipValue;
	double fraction = 0.0;
	if(paymentFraction == 0.05)
		fraction = 0.5;
	else if(paymentFraction == 0.1)
		fraction = 0.6;
	else if(paymentFraction == 0.15)
		fraction = 0.7;
	else if(paymentFraction == 0.2)
		fraction = 0.8;
	else if(paymentFraction == 0.25)
		fraction = 0.9;
	else if(paymentFraction == 0.3)
		fraction = 1.0;

	if(fraction == 0.0)
		return new PyNone;

	//  let the player pay for the insurance
	call.client->AddBalance(-payment);

	// delete old insurance, if any
	m_db->DeleteInsuranceByShipID(shipID);

	// add new insurance
	m_db->InsertInsuranceByShipID(shipID, fraction);

	return new PyNone;
}
