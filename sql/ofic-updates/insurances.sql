CREATE TABLE IF NOT EXISTS `insurances` (
  `shipID` int(10) NOT NULL,
  `startDate` bigint(20) NOT NULL,
  `endDate` bigint(20) NOT NULL,
  `fraction` float NOT NULL,
  PRIMARY KEY (`shipID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
